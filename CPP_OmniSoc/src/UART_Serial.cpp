#include "UART_Serial.h"

UART_Serial::UART_Serial(const std::string& port, unsigned int baud_rate, int timeoutPeriod_ms)
    : serial_(io_service_), port_(port), baud_rate_(baud_rate), timeoutPeriod_ms_(timeoutPeriod_ms), running_(false) {}

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

    startWorkThreads();
}

void UART_Serial::disconnect() {
    timeoutFlag = true;
    serial_.cancel();
    stopWorkThreads();
    if (serial_.is_open()) {
        serial_.close();
    }
}

bool UART_Serial::isConnected() {
    return !timeoutFlag;
}

size_t UART_Serial::available() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return buffer_.size();
}

void UART_Serial::flushIncomingSerial() {
    auto tempTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - tempTime < std::chrono::microseconds(byteSpacingTime_us * 2)) {
        if (available() > 0) {
            std::lock_guard<std::mutex> lock(buffer_mutex_); //releases on scope drop
            buffer_.clear();
            tempTime = std::chrono::steady_clock::now();
        }
    }
}

bool UART_Serial::asyncFlushIncomingSerial() {
    
    while (available() > 0) {
        std::lock_guard<std::mutex> lock(buffer_mutex_); //releases on scope drop
        buffer_.clear();
        asyncFlushClock = std::chrono::steady_clock::now();
    }
    
    if (std::chrono::steady_clock::now() - asyncFlushClock > std::chrono::microseconds(byteSpacingTime_us * 2)) {
        return true;
    }

    return false;
}

int UART_Serial::sendMessage(int header, const std::vector<float>& data) {
    int numFloats = data.size();
    uint8_t messageSize = HEADER_SIZE + 1 + numFloats * FLOAT_SIZE + CHECKSUM_SIZE;
    std::vector<uint8_t> message(messageSize);

    message[0] = (header >> 8) & 0xFF;
    message[1] = header & 0xFF;
    message[2] = (uint8_t)numFloats;

    for (int i = 0; i < numFloats; ++i) {
        /*
        const uint8_t* floatBytes = reinterpret_cast<const uint8_t*>(&data[i]);
        for (int j = 0; j < FLOAT_SIZE; ++j) {
            message[HEADER_SIZE + 1 + i * FLOAT_SIZE + j] = floatBytes[j];
        }
        */
        memcpy(&message[HEADER_SIZE + 1 + i * FLOAT_SIZE], &data[i], FLOAT_SIZE);
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
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (!timeoutFlag && std::chrono::steady_clock::now() - lastTimeoutClock > std::chrono::milliseconds(timeoutPeriod_ms_)) {
        timeoutFlag = true;
    }

    if (seekingFlag) {
        return -5;
    }

    if (lastHeader == -1) {
        if (buffer_.size() < HEADER_SIZE + 1) {
            return 0;
        }

        std::vector<uint8_t> headerBytes(buffer_.begin(), buffer_.begin() + HEADER_SIZE + 1);
        lastHeader = (headerBytes[0] << 8) | headerBytes[1];
        lastNumFloats = headerBytes[2];
        buffer_.erase(buffer_.begin(), buffer_.begin() + HEADER_SIZE + 1);
        //std::cout << "last header:" << lastHeader <<" , num floats:"<<lastNumFloats<< std::endl;

        if (lastNumFloats > maxFloats)
        {
            setSeekingFlag();
            lastHeader = -1;
            lastNumFloats = 0;
            return -4;
        }
    }
    else
    {
        header = lastHeader;
    }

    int floatDataSize = lastNumFloats * FLOAT_SIZE + CHECKSUM_SIZE;
    if (buffer_.size() < floatDataSize) {
        return -2; //wait for complete message
    }

    int messageSize = floatDataSize + HEADER_SIZE + 1;

    std::vector<uint8_t> message(buffer_.begin(), buffer_.begin() + floatDataSize);
    buffer_.erase(buffer_.begin(), buffer_.begin() + floatDataSize);


    data.resize(lastNumFloats);
    for (int i = 0; i < lastNumFloats; ++i) {
        data[i] = *reinterpret_cast<float*>(&message[i * FLOAT_SIZE]);
    }

    //add message header to front of message for checksum
    uint8_t headerByte1 = (uint8_t)(lastHeader & 0xFF);
    uint8_t headerByte2 = (uint8_t)((lastHeader >> 8) & 0xFF);
    message.insert(message.begin(), (uint8_t)lastNumFloats);
    message.insert(message.begin(), headerByte2);
    message.insert(message.begin(), headerByte1);

    lastHeader = -1;
    lastNumFloats = 0;

    uint8_t checksum = computeChecksum(message.data(), messageSize - CHECKSUM_SIZE);
    //std::cout << "checksum: " << checksum << " ; " << message[messageSize - 1] << std::endl;
    if (checksum != message[messageSize - 1]) {
        setSeekingFlag();
        return -3;
    }

    timeoutFlag = false;
    lastTimeoutClock = std::chrono::steady_clock::now();
    return 1;
}

uint8_t UART_Serial::computeChecksum(const uint8_t* data, int size) {
    uint8_t checksum = 0;
    for (int i = 0; i < size; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

void UART_Serial::readFromSerial() {
    while (running_) {
        char temp[1024];
        boost::system::error_code ec;
        std::size_t bytes_read = serial_.read_some(boost::asio::buffer(temp), ec);

        if (ec && ec != boost::asio::error::would_block) {
            std::cerr << "Error reading from serial port: " << ec.message() << std::endl;
            continue;
        }

        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.insert(buffer_.end(), temp, temp + bytes_read);
        //TODO: this could technically overfill if read is not being called...


        std::this_thread::sleep_for(std::chrono::microseconds(5 * byteSpacingTime_us));//this is kinda arbitrary (could be specified)
    }
}

void UART_Serial::handleSynchronization() {
    //byteSpacingTime_us
    while (running_) {
        if (seekingFlag) {
            bool flushResult = asyncFlushIncomingSerial();
            seekingFlag = !flushResult;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(byteSpacingTime_us));
    }
}

void UART_Serial::startWorkThreads() {
    running_ = true;
    read_thread_ = std::thread(&UART_Serial::readFromSerial, this);
    sync_thread_ = std::thread(&UART_Serial::handleSynchronization, this);
}

void UART_Serial::stopWorkThreads() {
    running_ = false;
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }
}

