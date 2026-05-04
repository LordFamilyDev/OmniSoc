#include "UART_Serial.h"

#if defined(__unix__) || defined(__APPLE__)
#  include <termios.h>
#  include <unistd.h>
#endif

UART_Serial::UART_Serial(const std::string& port, unsigned int baud_rate, int timeoutPeriod_ms)
    : serial_(io_context_), port_(port), baud_rate_(baud_rate), timeoutPeriod_ms_(timeoutPeriod_ms), running_(false) {}

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
    serial_.set_option(boost::asio::serial_port_base::character_size(8));
    serial_.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
    serial_.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
    serial_.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

#if defined(__unix__) || defined(__APPLE__)
    // Put the TTY in raw mode. Boost ASIO's flow_control::none only clears
    // hardware RTS/CTS; it leaves ICANON (line buffering) and IXON/IXOFF
    // (software flow control) at the kernel default, which is typically ON.
    // With IXON on, binary bytes that happen to equal XOFF (0x13) silently
    // stall writes until an XON (0x11) byte arrives — deadlocking sendMessage()
    // under sustained traffic. With ICANON on, the kernel withholds bytes
    // from read() until a newline arrives. cfmakeraw() disables both plus
    // signal chars, echo, and output post-processing.
    int fd = serial_.native_handle();
    termios tio{};
    if (tcgetattr(fd, &tio) == 0) {
        cfmakeraw(&tio);
        tcsetattr(fd, TCSANOW, &tio);
    }
#endif

    byteSpacingTime_us = static_cast<long>(ceil(10000000.0 / baud_rate_));
    asyncFlushClock = std::chrono::steady_clock::now();
    lastTimeoutClock = std::chrono::steady_clock::now();
    timeoutFlag = false;

    startWorkThreads();
}

void UART_Serial::disconnect() {
    timeoutFlag = true;
    running_ = false;

    // Close the fd first so any thread blocked inside serial_.read_some()
    // unblocks with an error and can observe running_=false. Without this
    // the read thread stays parked in a kernel read() forever and
    // stopWorkThreads() -> join() deadlocks, leaving a zombie process.
    boost::system::error_code ec;
    if (serial_.is_open()) {
        serial_.cancel(ec);
        serial_.close(ec);
    }

    stopWorkThreads();
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
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (!buffer_.empty()) {
        buffer_.clear();
        asyncFlushClock = std::chrono::steady_clock::now();
    }
    return std::chrono::steady_clock::now() - asyncFlushClock > std::chrono::microseconds(byteSpacingTime_us * 2);
}

int UART_Serial::sendMessage(int header, const std::vector<float>& data) {
    int numFloats = data.size();
    int messageSize = SYNC_SIZE + HEADER_SIZE + 1 + numFloats * FLOAT_SIZE + CRC_SIZE;
    std::vector<uint8_t> message(messageSize);

    // Sync bytes (advisory pre-filter for the receiver — not in the CRC).
    message[0] = SYNC_0;
    message[1] = SYNC_1;

    // Header (big-endian).
    message[SYNC_SIZE + 0] = (header >> 8) & 0xFF;
    message[SYNC_SIZE + 1] = header & 0xFF;

    // numFloats.
    message[SYNC_SIZE + HEADER_SIZE] = (uint8_t)numFloats;

    // Float payload.
    for (int i = 0; i < numFloats; ++i) {
        memcpy(&message[SYNC_SIZE + HEADER_SIZE + 1 + i * FLOAT_SIZE],
               &data[i], FLOAT_SIZE);
    }

    // CRC-16 over [hdr_hi, hdr_lo, numFloats, float_bytes] — sync excluded.
    uint16_t crc = crc16_ccitt(&message[SYNC_SIZE],
                               HEADER_SIZE + 1 + numFloats * FLOAT_SIZE);
    message[messageSize - 2] = (uint8_t)(crc & 0xFF);          // little-endian
    message[messageSize - 1] = (uint8_t)((crc >> 8) & 0xFF);

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
    if (!timeoutFlag && !seekingFlag &&
        std::chrono::steady_clock::now() - lastTimeoutClock > std::chrono::milliseconds(timeoutPeriod_ms_)) {
        timeoutFlag = true;
    }

    if (seekingFlag) {
        return -5;
    }

    int lastStatus = -1;

    // Sync-scan loop. On any per-frame failure we drop the leading sync (or
    // the trailing junk) and rescan. Mirrors the Arduino implementation.
    while ((int)buffer_.size() >= SYNC_SIZE) {
        // Find the next 0xA5 0x5A.
        int n = (int)buffer_.size();
        int syncIdx = -1;
        for (int i = 0; i + 1 < n; i++) {
            if ((uint8_t)buffer_[i] == SYNC_0 && (uint8_t)buffer_[i + 1] == SYNC_1) {
                syncIdx = i;
                break;
            }
        }

        if (syncIdx < 0) {
            // No sync anywhere. Drop everything except the trailing byte (it
            // might be a 0xA5 starting a sync that hasn't completed yet).
            if (n > 1) {
                char last = buffer_[n - 1];
                buffer_.clear();
                buffer_.push_back(last);
            }
            return lastStatus;
        }

        // Drop pre-sync junk.
        if (syncIdx > 0) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + syncIdx);
        }

        // Need at least sync + header + count = 5 bytes to look at numFloats.
        if ((int)buffer_.size() < SYNC_SIZE + HEADER_SIZE + 1) {
            return -1;
        }

        int hdr = ((uint8_t)buffer_[SYNC_SIZE] << 8) | (uint8_t)buffer_[SYNC_SIZE + 1];
        int nf  = (uint8_t)buffer_[SYNC_SIZE + HEADER_SIZE];

        if (nf < 0 || nf > maxFloats) {
            // Implausible numFloats — false sync. Drop the sync bytes only.
            buffer_.erase(buffer_.begin(), buffer_.begin() + SYNC_SIZE);
            lastStatus = -4;
            continue;
        }

        int total = SYNC_SIZE + HEADER_SIZE + 1 + nf * FLOAT_SIZE + CRC_SIZE;
        if ((int)buffer_.size() < total) {
            // Frame not fully arrived yet. Come back next call.
            return -2;
        }

        // CRC over [hdr_hi, hdr_lo, numFloats, float_bytes] — sync excluded.
        uint16_t computed = crc16_ccitt((const uint8_t*)&buffer_[SYNC_SIZE],
                                          HEADER_SIZE + 1 + nf * FLOAT_SIZE);
        uint16_t received = (uint16_t)(uint8_t)buffer_[total - 2]
                          | ((uint16_t)(uint8_t)buffer_[total - 1] << 8);

        if (computed != received) {
            // False sync match or real frame got corrupted. Drop the sync;
            // rescan finds the next.
            buffer_.erase(buffer_.begin(), buffer_.begin() + SYNC_SIZE);
            lastStatus = -3;
            continue;
        }

        // Valid frame.
        header = hdr;
        data.resize(nf);
        for (int i = 0; i < nf; i++) {
            memcpy(&data[i],
                   &buffer_[SYNC_SIZE + HEADER_SIZE + 1 + i * FLOAT_SIZE],
                   FLOAT_SIZE);
        }
        buffer_.erase(buffer_.begin(), buffer_.begin() + total);

        timeoutFlag = false;
        lastTimeoutClock = std::chrono::steady_clock::now();
        return 1;
    }

    return lastStatus;
}

uint16_t UART_Serial::crc16_ccitt(const uint8_t* data, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; ++i) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
            else              crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void UART_Serial::readFromSerial() {
    while (running_) {
        char temp[1024];
        boost::system::error_code ec;
        std::size_t bytes_read = serial_.read_some(boost::asio::buffer(temp), ec);

        if (ec && ec != boost::asio::error::would_block) {
            std::cerr << "Error reading from serial port: " << ec.message() << std::endl;
            // Back off after a persistent error (e.g. device disappeared,
            // repeated EOF) so we don't burn a core and flood stderr.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.insert(buffer_.end(), temp, temp + bytes_read);
        //TODO: this could technically overfill if read is not being called...


        std::this_thread::sleep_for(std::chrono::microseconds(5 * byteSpacingTime_us));//this is kinda arbitrary (could be specified)
    }
}

void UART_Serial::handleSynchronization() {
    while (running_) {
        if (seekingFlag) {
            bool flushResult = asyncFlushIncomingSerial();
            if (flushResult) {
                // Reset timeout clock before clearing seekingFlag so receiveMessage()
                // never sees seekingFlag=false with a stale lastTimeoutClock.
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex_);
                    lastTimeoutClock = std::chrono::steady_clock::now();
                }
                seekingFlag = false;
            }
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

