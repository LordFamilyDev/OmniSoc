import serial
import struct
import time
import random

class SerialManager:
    HEADER_SIZE = 2
    CHECKSUM_SIZE = 1
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

    def compute_checksum(self, data):
        """Compute XOR checksum of the data"""
        checksum = 0
        for byte in data:
            checksum ^= byte
        return checksum

    def send_message(self, header, data):
        """Send a message with header and float data"""
        if not self.serial.is_open:
            return -1

        num_floats = len(data)
        if num_floats > self.MAX_FLOATS:
            print(f"Error: Too many floats ({num_floats} > {self.MAX_FLOATS})")
            return -1

        # Create message header
        message = bytearray()
        message.extend(header.to_bytes(2, 'big'))  # 2-byte header
        message.append(num_floats)  # 1-byte float count

        # Add float data
        for value in data:
            message.extend(struct.pack('f', value))  # 4-byte float

        # Add checksum
        checksum = self.compute_checksum(message)
        message.append(checksum)

        try:
            bytes_written = self.serial.write(message)
            return 1 if bytes_written == len(message) else -1
        except serial.SerialException as e:
            print(f"Send error: {e}")
            return -1

    def receive_message(self):
        """Receive and parse a message, returns (header, data) tuple"""
        if not self.serial.is_open:
            return None, None

        # Read header (2 bytes) and number of floats (1 byte)
        header_data = self.serial.read(self.HEADER_SIZE + 1)
        if len(header_data) < self.HEADER_SIZE + 1:
            return None, None

        header = int.from_bytes(header_data[:2], 'big')
        num_floats = header_data[2]

        if num_floats > self.MAX_FLOATS:
            print(f"Error: Received too many floats ({num_floats})")
            self.flush_incoming()
            return None, None

        # Read float data and checksum
        data_size = num_floats * self.FLOAT_SIZE + self.CHECKSUM_SIZE
        data = self.serial.read(data_size)
        if len(data) < data_size:
            return None, None

        # Verify checksum
        message = header_data + data[:-1]
        received_checksum = data[-1]
        computed_checksum = self.compute_checksum(message)

        if received_checksum != computed_checksum:
            print("Checksum error")
            self.flush_incoming()
            return None, None

        # Parse float data
        float_data = []
        for i in range(num_floats):
            start = i * self.FLOAT_SIZE
            end = start + self.FLOAT_SIZE
            value = struct.unpack('f', data[start:end])[0]
            float_data.append(value)

        self.timeout_flag = False
        self.last_timeout_clock = time.time() * 1000
        return header, float_data

    def flush_incoming(self):
        """Clear any remaining data in the input buffer"""
        if self.serial.is_open:
            self.serial.reset_input_buffer()

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