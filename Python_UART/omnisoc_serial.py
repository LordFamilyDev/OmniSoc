import serial
import struct
import time
import random

# OmniSoc UART framing v3:
#   [0xA5][0x5A][hdr:1][len:1][bytes:0..MAX_PAYLOAD][crc16_lo][crc16_hi]
#
# Sync bytes are an advisory pre-filter (payload bytes can collide); CRC-16
# (CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout) computed
# over [hdr, len, bytes] is the actual frame-validity authority.
#
# Payload is opaque bytes. Use struct.pack/unpack with little-endian format
# strings ('<I', '<f', '<H' etc.) to compose typed fields. Mirrors the C++
# and Arduino PackBytes.h helpers — same wire bytes.
#
# Total frame size: 6 + len bytes (max 6 + 48 = 54).


def _crc16_ccitt(data):
    """CRC-16/CCITT-FALSE. crc16_ccitt(b'123456789') == 0x29B1."""
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


class SerialManager:
    SYNC_0 = 0xA5
    SYNC_1 = 0x5A
    SYNC_BYTES = bytes([SYNC_0, SYNC_1])
    SYNC_SIZE = 2
    HEADER_SIZE = 1
    LEN_SIZE = 1
    CRC_SIZE = 2
    FRAME_OVERHEAD = SYNC_SIZE + HEADER_SIZE + LEN_SIZE + CRC_SIZE  # 6
    MAX_PAYLOAD = 48
    MAX_FLOATS = MAX_PAYLOAD // 4  # 12

    def __init__(self, port='/dev/ttyUSB0', timeout_ms=20, tx_pacing_enabled=True):
        """Initialize the serial manager with port and timeout settings.

        tx_pacing_enabled: if True (default), send_message() blocks between
        consecutive sends so that frames are delivered to the receiver no
        faster than the baud rate can carry them. Prevents back-to-back
        bursts from overflowing a slow receiver's HW UART buffer (Arduino
        is 64 B). Disable only if you're sure your receiver can keep up.
        """
        self.serial = serial.Serial()
        self.serial.port = port
        self.serial.timeout = timeout_ms / 1000.0  # Convert to seconds
        self.timeout_period_ms = timeout_ms
        self.last_timeout_clock = time.time() * 1000
        self.timeout_flag = True
        self._rx_buf = bytearray()
        self._scan_pos = 0
        self._tx_pacing_enabled = tx_pacing_enabled
        self._byte_spacing_us = 0  # set in connect()
        self._earliest_next_send = 0.0  # monotonic seconds

    def connect(self, baud_rate=57600):
        """Connect to the serial port with specified baud rate"""
        self.serial.baudrate = baud_rate
        # 10 bits per byte (1 start + 8 data + 1 stop) — wire time per byte.
        self._byte_spacing_us = 10_000_000.0 / baud_rate
        try:
            if self.serial.is_open:
                self.serial.close()
            self.serial.open()
            print(f"Connected to {self.serial.port} at {baud_rate} baud")
            return True
        except serial.SerialException as e:
            print(f"Failed to connect: {e}")
            return False

    def disconnect(self):
        """Close the serial connection"""
        if self.serial.is_open:
            self.serial.close()
            print("Disconnected from serial port")

    def is_connected(self):
        """Check if connection is active and not timed out"""
        return not self.timeout_flag and self.serial.is_open

    def send_message(self, header, data):
        """Send an OmniSoc v3 frame with 1-byte header and opaque byte payload.

        Args:
            header: int 0..255
            data:   bytes-like, 0..MAX_PAYLOAD bytes

        Returns 1 on success, -1 on failure.
        """
        if not self.serial.is_open:
            return -1
        if not (0 <= header <= 0xFF):
            return -1
        n = len(data)
        if n > self.MAX_PAYLOAD:
            return -1

        message = bytearray()
        message.append(self.SYNC_0)
        message.append(self.SYNC_1)
        message.append(header)
        message.append(n)
        message.extend(data)
        crc = _crc16_ccitt(message[self.SYNC_SIZE:])
        message.append(crc & 0xFF)
        message.append((crc >> 8) & 0xFF)

        # TX self-pacing: don't dispatch until the previous frame has had time
        # to fully shift out at the baud rate. Caps the effective on-wire byte
        # rate at the baud rate, so a slow receiver's HW UART buffer can never
        # be overflowed by sender bursting.
        if self._tx_pacing_enabled and self._byte_spacing_us > 0:
            now = time.monotonic()
            if now < self._earliest_next_send:
                time.sleep(self._earliest_next_send - now)

        try:
            written = self.serial.write(message)
        except serial.SerialException as e:
            print(f"Send error: {e}")
            return -1

        if self._tx_pacing_enabled and self._byte_spacing_us > 0:
            wire_time_s = (len(message) * self._byte_spacing_us) / 1_000_000.0
            self._earliest_next_send = time.monotonic() + wire_time_s

        return 1 if written == len(message) else -1

    def send_message_floats(self, header, floats):
        """Convenience wrapper: pack a sequence of floats little-endian and send."""
        if len(floats) > self.MAX_FLOATS:
            return -1
        data = struct.pack('<' + 'f' * len(floats), *floats)
        return self.send_message(header, data)

    def receive_message(self):
        """Receive and parse one v3 frame.

        Returns (header: int, data: bytes) on success, or (None, None) if no
        complete valid frame is available yet.
        """
        if not self.serial.is_open:
            return None, None

        # Drain whatever the port has into our internal buffer (non-blocking).
        if self.serial.in_waiting:
            self._rx_buf.extend(self.serial.read(self.serial.in_waiting))

        while True:
            # Compact if scan_pos has crept too far forward.
            if self._scan_pos > 0 and self._scan_pos > len(self._rx_buf) // 2:
                del self._rx_buf[:self._scan_pos]
                self._scan_pos = 0

            if len(self._rx_buf) < self._scan_pos + self.SYNC_SIZE:
                return None, None

            # Find next 0xA5 0x5A starting at scan_pos.
            sync_idx = self._rx_buf.find(self.SYNC_BYTES, self._scan_pos)

            if sync_idx < 0:
                # No sync. Preserve the trailing byte (could be a 0xA5 with
                # 0x5A still pending in the kernel buffer).
                if len(self._rx_buf) > 1:
                    self._scan_pos = len(self._rx_buf) - 1
                else:
                    self._scan_pos = 0
                return None, None

            # Need sync + header + len = 4 bytes minimum.
            if len(self._rx_buf) < sync_idx + self.SYNC_SIZE + self.HEADER_SIZE + self.LEN_SIZE:
                self._scan_pos = sync_idx
                return None, None

            hdr = self._rx_buf[sync_idx + self.SYNC_SIZE]
            plen = self._rx_buf[sync_idx + self.SYNC_SIZE + self.HEADER_SIZE]

            if plen > self.MAX_PAYLOAD:
                # False sync. Advance one byte and rescan.
                self._scan_pos = sync_idx + 1
                continue

            total = sync_idx + self.FRAME_OVERHEAD + plen
            if len(self._rx_buf) < total:
                # Frame not fully arrived yet.
                self._scan_pos = sync_idx
                return None, None

            # CRC over [hdr][len][bytes] — sync excluded.
            payload_start = sync_idx + self.SYNC_SIZE
            payload_end = total - self.CRC_SIZE
            computed = _crc16_ccitt(bytes(self._rx_buf[payload_start:payload_end]))
            received = self._rx_buf[total - 2] | (self._rx_buf[total - 1] << 8)

            if computed != received:
                # False sync match or corrupted frame. Advance past this sync byte.
                self._scan_pos = sync_idx + 1
                continue

            # Valid frame.
            data_offset = sync_idx + self.SYNC_SIZE + self.HEADER_SIZE + self.LEN_SIZE
            data = bytes(self._rx_buf[data_offset:data_offset + plen])
            del self._rx_buf[:total]
            self._scan_pos = 0

            self.timeout_flag = False
            self.last_timeout_clock = time.time() * 1000
            return hdr, data

    def receive_message_floats(self):
        """Convenience wrapper: receive a frame and unpack as floats.

        Returns (header, [float, ...]) on success, (None, None) if no frame,
        or (header, None) if the payload length isn't a multiple of 4 bytes.
        """
        hdr, data = self.receive_message()
        if hdr is None:
            return None, None
        if len(data) % 4 != 0:
            return hdr, None
        n = len(data) // 4
        floats = list(struct.unpack('<' + 'f' * n, data))
        return hdr, floats

    def flush_incoming(self):
        """Drop all buffered serial input + reset internal parser state.

        Use after mode switches or when the application detects prolonged
        corruption and wants to start fresh. Not used for automatic resync —
        CRC-16 handles that.
        """
        if self.serial.is_open:
            self.serial.reset_input_buffer()
        self._rx_buf.clear()
        self._scan_pos = 0


def main():
    # Example usage
    manager = SerialManager('COM5')  # Adjust port as needed
    if not manager.connect(57600):
        return

    try:
        print("Starting communication test...")
        while True:
            # Send a mixed-type telemetry message: uint32 tick + float angle + uint32 ms.
            payload = struct.pack('<IfI', int(time.time()), 1.23, int(time.time() * 1000) & 0xFFFFFFFF)
            result = manager.send_message(1, payload)
            if result > 0:
                print(f"Sent telemetry ({len(payload)} bytes)")

            # Receive any incoming messages.
            header, data = manager.receive_message()
            if header is not None:
                print(f"Received - Header: {header}, Data ({len(data)} B): {data.hex()}")

            # Sporadic debug message: single uint16 status code.
            if random.random() < 0.1:
                manager.send_message(5, struct.pack('<H', 5000))
                print("Sent debug message")

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        manager.disconnect()


if __name__ == "__main__":
    main()
