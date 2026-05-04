import serial
import struct
import time
import random

# OmniSoc UART framing v2:
#   [0xA5][0x5A][hdr_hi][hdr_lo][numFloats][float bytes][crc16_lo][crc16_hi]
# Sync bytes are an advisory pre-filter (float bytes can collide); CRC-16
# (CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout) computed
# over [hdr, numFloats, floats] is the actual frame-validity authority.

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
    SYNC_SIZE = 2
    HEADER_SIZE = 2
    CRC_SIZE = 2
    FLOAT_SIZE = 4
    MAX_FLOATS = 10

    def __init__(self, port='/dev/ttyUSB0', timeout_ms=20):
        """Initialize the serial manager with port and timeout settings"""
        self.serial = serial.Serial()
        self.serial.port = port
        self.serial.timeout = timeout_ms / 1000.0  # Convert to seconds
        self.timeout_period_ms = timeout_ms
        self.last_timeout_clock = time.time() * 1000
        self.timeout_flag = True
        self.seeking_flag = True
        self._rx_buf = bytearray()

    def connect(self, baud_rate=57600):
        """Connect to the serial port with specified baud rate"""
        self.serial.baudrate = baud_rate
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
        """Send an OmniSoc v2 frame with header and float data."""
        if not self.serial.is_open:
            return -1

        num_floats = len(data)
        if num_floats > self.MAX_FLOATS:
            print(f"Error: Too many floats ({num_floats} > {self.MAX_FLOATS})")
            return -1

        message = bytearray()
        # Sync bytes (advisory pre-filter — not in CRC).
        message.append(self.SYNC_0)
        message.append(self.SYNC_1)
        # Header (big-endian).
        message.extend(header.to_bytes(2, 'big'))
        # numFloats.
        message.append(num_floats)
        # Float payload.
        for value in data:
            message.extend(struct.pack('<f', value))
        # CRC over [hdr, numFloats, floats] — sync excluded. Stored little-endian.
        crc = _crc16_ccitt(message[self.SYNC_SIZE:])
        message.append(crc & 0xFF)
        message.append((crc >> 8) & 0xFF)

        try:
            bytes_written = self.serial.write(message)
            return 1 if bytes_written == len(message) else -1
        except serial.SerialException as e:
            print(f"Send error: {e}")
            return -1

    def receive_message(self):
        """Receive and parse one frame; returns (header, data) or (None, None).

        Maintains an internal byte buffer. Each call drains any new bytes from
        the serial port, scans for the sync sequence, then validates with CRC.
        Mirrors the Arduino/CPP parser semantics.
        """
        if not self.serial.is_open:
            return None, None

        # Drain whatever the port has into our internal buffer (non-blocking).
        if self.serial.in_waiting:
            self._rx_buf.extend(self.serial.read(self.serial.in_waiting))

        while len(self._rx_buf) >= self.SYNC_SIZE:
            # Find next 0xA5 0x5A.
            n = len(self._rx_buf)
            sync_idx = -1
            for i in range(n - 1):
                if self._rx_buf[i] == self.SYNC_0 and self._rx_buf[i + 1] == self.SYNC_1:
                    sync_idx = i
                    break

            if sync_idx < 0:
                # No sync. Drop everything except the trailing byte (might be
                # 0xA5 starting a sync that hasn't completed yet).
                if n > 1:
                    last = self._rx_buf[-1]
                    self._rx_buf.clear()
                    self._rx_buf.append(last)
                return None, None

            # Drop pre-sync junk.
            if sync_idx > 0:
                del self._rx_buf[:sync_idx]

            # Need sync + header + count = 5 bytes minimum.
            if len(self._rx_buf) < self.SYNC_SIZE + self.HEADER_SIZE + 1:
                return None, None

            hdr = (self._rx_buf[self.SYNC_SIZE] << 8) | self._rx_buf[self.SYNC_SIZE + 1]
            nf = self._rx_buf[self.SYNC_SIZE + self.HEADER_SIZE]

            if nf > self.MAX_FLOATS:
                # Implausible numFloats — false sync. Drop the sync bytes only.
                del self._rx_buf[:self.SYNC_SIZE]
                continue

            total = self.SYNC_SIZE + self.HEADER_SIZE + 1 + nf * self.FLOAT_SIZE + self.CRC_SIZE
            if len(self._rx_buf) < total:
                return None, None  # frame not fully arrived

            # CRC over [hdr, numFloats, floats] — sync excluded.
            payload = bytes(self._rx_buf[self.SYNC_SIZE:total - self.CRC_SIZE])
            computed = _crc16_ccitt(payload)
            received = self._rx_buf[total - 2] | (self._rx_buf[total - 1] << 8)

            if computed != received:
                # False sync match or real frame got corrupted. Drop sync;
                # rescan finds the next.
                del self._rx_buf[:self.SYNC_SIZE]
                continue

            # Valid frame.
            float_bytes_offset = self.SYNC_SIZE + self.HEADER_SIZE + 1
            float_data = []
            for i in range(nf):
                start = float_bytes_offset + i * self.FLOAT_SIZE
                end = start + self.FLOAT_SIZE
                value = struct.unpack('<f', bytes(self._rx_buf[start:end]))[0]
                float_data.append(value)
            del self._rx_buf[:total]

            self.timeout_flag = False
            self.last_timeout_clock = time.time() * 1000
            return hdr, float_data

        return None, None

    def flush_incoming(self):
        """Clear any remaining data in the input buffer"""
        if self.serial.is_open:
            self.serial.reset_input_buffer()
        self._rx_buf.clear()

def main():
    # Example usage
    manager = SerialManager('COM5')  # Adjust port as needed
    if not manager.connect(57600):
        return

    try:
        print("Starting communication test...")
        while True:
            # Send a test message
            test_data = [1.23, 4.56, time.time()]
            result = manager.send_message(1, test_data)
            if result > 0:
                print(f"Sent message with data: {test_data}")

            # Receive any incoming messages
            header, data = manager.receive_message()
            if header is not None:
                print(f"Received message - Header: {header}, Data: {data}")

            # Random debug message (similar to Arduino example)
            if random.random() < 0.1:  # 10% chance each loop
                debug_data = [5000.0]
                manager.send_message(5, debug_data)
                print("Sent debug message")

            time.sleep(0.01)  # 10ms delay to match Arduino example

    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        manager.disconnect()

if __name__ == "__main__":
    main()