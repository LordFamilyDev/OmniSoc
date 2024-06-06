#include "UART_Serial.h"

UART_Serial::UART_Serial(const std::string& port, unsigned int baud_rate, int timeoutPeriod_ms)
    : serial_(io_service_), port_(port), baud_rate_(baud_rate), timeoutPeriod_ms_(timeoutPeriod_ms) {}

UART_Serial::~UART_Serial() {
    disconnect();
}

void UART_Serial::connect() {
    boost::system::error_code ec;
    serial_.open(port_, ec);
    if (ec) {
        std::cerr << "Error opening serial port: " << ec.message() << std::endl;
        return;
    }
    serial_.set_option(boost::asio::serial_port_base::baud_rate(baud_rate_));
    byteSpacingTime_us = static_cast<long>(ceil(10000000.0 / baud_rate_));
    timeoutFlag = false;
}

void UART_Serial::disconnect() {
    if (serial_.is_open()) {
        serial_.close();
    }
}

bool UART_Serial::isConnected() {
    return !timeoutFlag;
}

void UART_Serial::flushIncomingSerial() {
    auto tempTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - tempTime < std::chrono::microseconds(byteSpacingTime_us * 2)) {
        if (serial_.is_open() && boost::asio::read(serial_, buffer_, boost::asio::transfer_at_least(1), boost::system::error_code())) {
            tempTime = std::chrono::steady_clock::now();
        }
    }
}

bool UART_Serial::asyncFlushIncomingSerial() {
    if (std::chrono::steady_clock::now() - asyncFlushClock > std::chrono::microseconds(byteSpacingTime_us * 2)) {
        if (std::chrono::steady_clock::now() - asyncFlushClock > std::chrono::milliseconds(timeoutPeriod_ms_)) {
            asyncFlushClock = std::chrono::steady_clock::now();
        }
        else {
            return true;
        }
    }

    if (serial_.is_open() && boost::asio::read(serial_, buffer_, boost::asio::transfer_at_least(1), boost::system::error_code())) {
        asyncFlushClock = std::chrono::steady_clock::now();
    }

    return false;
}

void UART_Serial::handleSynchronization() {
    if (seekingFlag) {
        bool flushResult = asyncFlushIncomingSerial();
        seekingFlag = !flushResult;
    }
}

int UART_Serial::sendMessage(int header, const std::vector<float>& data) {
    int numFloats = data.size();
    uint8_t messageSize = HEADER_SIZE + 1 + numFloats * FLOAT_SIZE + CHECKSUM_SIZE;
    std::vector<uint8_t> message(messageSize);

    message[0] = (header >> 8) & 0xFF;
    message[1] = header & 0xFF;
    message[2] = numFloats;

    for (int i = 0; i < numFloats; ++i) {
        const uint8_t* floatBytes = reinterpret_cast<const uint8_t*>(&data[i]);
        for (int j = 0; j < FLOAT_SIZE; ++j) {
            message[HEADER_SIZE + 1 + i * FLOAT_SIZE + j] = floatBytes[j];
        }
    }

    uint8_t checksum = computeChecksum(message.data(), messageSize - CHECKSUM_SIZE);
    message[messageSize - 1] = checksum;

    boost::system::error_code ec;
    boost::asio::write(serial_, boost::asio::buffer(message), ec);
    if (ec) {
        std::cerr << "Error writing to serial port: " << ec.message() << std::endl;
        return -1;
    }

    return 1;
}

int UART_Serial::receiveMessage(int& header, std::vector<float>& data) {
    if (timeoutFlag && std::chrono::steady_clock::now() - lastTimeoutClock > std::chrono::milliseconds(timeoutPeriod_ms_)) {
        timeoutFlag = true;
    }

    if (seekingFlag) {
        return -5;
    }

    if (lastHeader == -1) {
        if (buffer_.size() < HEADER_SIZE) {
            return 0;
        }

        std::istream is(&buffer_);
        uint8_t headerBytes[HEADER_SIZE];
        is.read(reinterpret_cast<char*>(headerBytes), HEADER_SIZE);
        lastHeader = (headerBytes[0] << 8) | headerBytes[1];
    }

    if (lastNumFloats == 0) {
        if (buffer_.size() < 1) {
            return 0;
        }

        std::istream is(&buffer_);
        char numFloats;
        is.read(&numFloats, 1);
        lastNumFloats = numFloats;
        if (lastNumFloats > 10) {
            seekingFlag = true;
            return -3;
        }
    }

    int messageSize = lastNumFloats * FLOAT_SIZE + CHECKSUM_SIZE;
    if (buffer_.size() < messageSize) {
        return 0;
    }

    std::istream is(&buffer_);
    std::vector<uint8_t> message(messageSize);
    is.read(reinterpret_cast<char*>(message.data()), messageSize);

    uint8_t checksum = computeChecksum(message.data(), messageSize - CHECKSUM_SIZE);
    if (checksum != message[messageSize - 1]) {
        seekingFlag = true;
        return -2;
    }

    data.resize(lastNumFloats);
    for (int i = 0; i < lastNumFloats; ++i) {
        data[i] = *reinterpret_cast<float*>(&message[i * FLOAT_SIZE]);
    }

    header = lastHeader;
    lastHeader = -1;
    lastNumFloats = 0;
    timeoutFlag = false;

    return 1;
}

uint8_t UART_Serial::computeChecksum(const uint8_t* data, int size) {
    uint8_t checksum = 0;
    for (int i = 0; i < size; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

