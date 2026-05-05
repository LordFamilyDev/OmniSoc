#include "UART_Serial.h"

#include <cmath>
#include <cstring>

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
    // Put the TTY in raw mode AND configure the kernel-side read timeout.
    //
    // Boost ASIO's flow_control::none only clears hardware RTS/CTS; it leaves
    // ICANON (line buffering) and IXON/IXOFF (software flow control) at the
    // kernel default, which is typically ON. With IXON on, binary bytes that
    // happen to equal XOFF (0x13) silently stall writes until an XON (0x11)
    // byte arrives — deadlocking sendMessage() under sustained traffic. With
    // ICANON on, the kernel withholds bytes from read() until a newline
    // arrives. cfmakeraw() disables both plus signal chars, echo, and output
    // post-processing.
    //
    // VMIN=0, VTIME=1 (100 ms) makes read() return within 100 ms with
    // whatever bytes are available (or zero on idle). This is the kernel-side
    // shutdown bound: the read thread observes running_=false within 100 ms
    // of disconnect() instead of blocking forever on an idle FD. Must be set
    // AFTER cfmakeraw() because cfmakeraw() resets VMIN=1, VTIME=0.
    int fd = serial_.native_handle();
    termios tio{};
    if (tcgetattr(fd, &tio) == 0) {
        cfmakeraw(&tio);
        tio.c_cc[VMIN]  = 0;
        tio.c_cc[VTIME] = 1;
        tcsetattr(fd, TCSANOW, &tio);
    }
#endif

    byteSpacingTime_us = static_cast<long>(ceil(10000000.0 / baud_rate_));
    lastTimeoutClock = std::chrono::steady_clock::now();
    timeoutFlag = false;

    startWorkThreads();
}

void UART_Serial::disconnect() {
    timeoutFlag = true;
    running_ = false;

    // VTIME bounds each kernel read at 100 ms, so the read thread observes
    // running_=false and exits cleanly. Join first, then close — no need to
    // race-close the FD to wake a blocked reader anymore.
    stopWorkThreads();

    boost::system::error_code ec;
    if (serial_.is_open()) {
        serial_.close(ec);
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
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    buffer_.clear();
    scan_pos_ = 0;
#if defined(__unix__) || defined(__APPLE__)
    if (serial_.is_open()) {
        tcflush(serial_.native_handle(), TCIFLUSH);
    }
#endif
}

int UART_Serial::sendMessage(uint8_t header, const uint8_t* bytes, uint8_t len) {
    if (len > MAX_PAYLOAD) {
        return -1;
    }

    int messageSize = FRAME_OVERHEAD + len;
    std::vector<uint8_t> message(messageSize);

    // Sync bytes (advisory pre-filter for the receiver — not in the CRC).
    message[0] = SYNC_0;
    message[1] = SYNC_1;
    message[SYNC_SIZE] = header;
    message[SYNC_SIZE + HEADER_SIZE] = len;
    if (len > 0) {
        std::memcpy(&message[SYNC_SIZE + HEADER_SIZE + LEN_SIZE], bytes, len);
    }

    // CRC-16 over [hdr][len][bytes] — sync excluded.
    uint16_t crc = crc16_ccitt(&message[SYNC_SIZE], HEADER_SIZE + LEN_SIZE + len);
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

int UART_Serial::sendMessage(uint8_t header, const float* data, uint8_t numFloats) {
    uint8_t lenBytes = numFloats * 4;
    if (lenBytes > MAX_PAYLOAD) {
        return -1;
    }
    uint8_t buf[MAX_PAYLOAD];
    if (numFloats > 0) {
        std::memcpy(buf, data, lenBytes);
    }
    return sendMessage(header, buf, lenBytes);
}

int UART_Serial::receiveMessage(uint8_t& header, uint8_t* bytes, uint8_t& len) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (!timeoutFlag &&
        std::chrono::steady_clock::now() - lastTimeoutClock > std::chrono::milliseconds(timeoutPeriod_ms_)) {
        timeoutFlag = true;
    }

    int lastStatus = -1;

    // Sync-scan loop with scan_pos_ offset — advance past false syncs without
    // memmoving the buffer. Compact only on valid frame extraction or when
    // scan_pos_ exceeds half the buffer size.
    while (true) {
        // Compact the leading garbage if scan_pos_ has crept too far forward.
        if (scan_pos_ > 0 && scan_pos_ > buffer_.size() / 2) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + scan_pos_);
            scan_pos_ = 0;
        }

        if (buffer_.size() < scan_pos_ + SYNC_SIZE) {
            return lastStatus;
        }

        // Find next 0xA5 0x5A starting at scan_pos_.
        size_t syncIdx = 0;
        bool found = false;
        for (size_t i = scan_pos_; i + 1 < buffer_.size(); ++i) {
            if (buffer_[i] == SYNC_0 && buffer_[i + 1] == SYNC_1) {
                syncIdx = i;
                found = true;
                break;
            }
        }

        if (!found) {
            // No sync. Preserve trailing byte in case it's a 0xA5 starting an
            // incomplete sync; everything before it is junk.
            if (buffer_.size() > 1) {
                scan_pos_ = buffer_.size() - 1;
            } else {
                scan_pos_ = 0;
            }
            return lastStatus;
        }

        // Need sync + header + len = 4 bytes minimum to inspect len.
        if (buffer_.size() < syncIdx + SYNC_SIZE + HEADER_SIZE + LEN_SIZE) {
            scan_pos_ = syncIdx;
            return -1;
        }

        uint8_t hdr = buffer_[syncIdx + SYNC_SIZE];
        uint8_t plen = buffer_[syncIdx + SYNC_SIZE + HEADER_SIZE];

        if (plen > MAX_PAYLOAD) {
            // Implausible len — false sync. Advance one byte.
            scan_pos_ = syncIdx + 1;
            lastStatus = -4;
            continue;
        }

        size_t total = syncIdx + FRAME_OVERHEAD + plen;
        if (buffer_.size() < total) {
            // Frame not fully arrived yet. Stay parked at this sync.
            scan_pos_ = syncIdx;
            return -2;
        }

        // CRC over [hdr][len][bytes] — sync excluded.
        uint16_t computed = crc16_ccitt(&buffer_[syncIdx + SYNC_SIZE], HEADER_SIZE + LEN_SIZE + plen);
        uint16_t received = (uint16_t)buffer_[total - 2]
                          | ((uint16_t)buffer_[total - 1] << 8);

        if (computed != received) {
            // False sync match or corrupted frame. Advance past this sync byte.
            scan_pos_ = syncIdx + 1;
            lastStatus = -3;
            continue;
        }

        // Valid frame.
        header = hdr;
        len = plen;
        if (plen > 0) {
            std::memcpy(bytes, &buffer_[syncIdx + SYNC_SIZE + HEADER_SIZE + LEN_SIZE], plen);
        }
        buffer_.erase(buffer_.begin(), buffer_.begin() + total);
        scan_pos_ = 0;

        timeoutFlag = false;
        lastTimeoutClock = std::chrono::steady_clock::now();
        return 1;
    }
}

int UART_Serial::receiveMessage(uint8_t& header, float* data, uint8_t& numFloats) {
    uint8_t buf[MAX_PAYLOAD];
    uint8_t len = 0;
    int rc = receiveMessage(header, buf, len);
    if (rc != 1) {
        numFloats = 0;
        return rc;
    }
    if (len % 4 != 0) {
        numFloats = 0;
        return -6;
    }
    numFloats = len / 4;
    if (numFloats > 0) {
        std::memcpy(data, buf, len);
    }
    return 1;
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
        uint8_t temp[1024];
        boost::system::error_code ec;
        std::size_t bytes_read = serial_.read_some(boost::asio::buffer(temp), ec);

        if (ec && ec != boost::asio::error::would_block) {
            // VTIME=1 makes idle reads return 0 bytes successfully; only a
            // genuine error (device removed, EOF on a closed FD) lands here.
            // Back off so we don't burn a core and flood stderr.
            std::cerr << "Error reading from serial port: " << ec.message() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (bytes_read > 0) {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_.insert(buffer_.end(), temp, temp + bytes_read);

            // Cap buffer at BUFFER_CAP — drop oldest half on overflow so the
            // consumer can recover via CRC instead of seeing unbounded growth.
            if (buffer_.size() > BUFFER_CAP) {
                size_t drop = buffer_.size() - BUFFER_CAP / 2;
                buffer_.erase(buffer_.begin(), buffer_.begin() + drop);
                scan_pos_ = 0;
                dropped_bytes_.fetch_add(drop);
            }

            // Hold the lock across the inter-iteration sleep. This is
            // load-bearing: releasing the lock per-iteration was tried in
            // commit dddd198 and tanked rx throughput from ~50 Hz to ~0.6 Hz
            // because there was no other consumer to grab it (the old sync
            // thread cleared the buffer mid-frame). Even with the sync thread
            // gone, a tight per-iteration lock-acquire pattern is wasteful;
            // batch under one lock.
            std::this_thread::sleep_for(std::chrono::microseconds(5 * byteSpacingTime_us));
        }
    }
}

void UART_Serial::startWorkThreads() {
    running_ = true;
    read_thread_ = std::thread(&UART_Serial::readFromSerial, this);
}

void UART_Serial::stopWorkThreads() {
    running_ = false;
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
}
