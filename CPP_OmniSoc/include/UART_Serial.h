#ifndef UART_SERIAL_H
#define UART_SERIAL_H

#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

class UART_Serial {
public:
    UART_Serial(const std::string& port, unsigned int baud_rate, int timeoutPeriod_ms);
    ~UART_Serial();

    void connect();
    void disconnect();
    bool isConnected();
    size_t available();

    //attempts to clear until the end of a message (blocking).
    void flushIncomingSerial();

    //flush asyncronously on a background thread (non-blocking)
    bool asyncFlushIncomingSerial();
    void handleSynchronization();

    int sendMessage(int header, const std::vector<float>& data);
    int receiveMessage(int& header, std::vector<float>& data);

    static const int maxFloats = 10;

private:
    void readFromSerial();
    void startWorkThreads();
    void stopWorkThreads();

    void setSeekingFlag()
    {
        seekingFlag = true;
        asyncFlushClock = std::chrono::steady_clock::now();
    }

    boost::asio::io_context io_context_;
    boost::asio::serial_port serial_;
    std::string port_;
    unsigned int baud_rate_;
    int timeoutPeriod_ms_;

    std::vector<char> buffer_;
    std::mutex buffer_mutex_;
    std::atomic<bool> running_;
    std::thread read_thread_;
    std::thread sync_thread_;

    // OmniSoc UART framing v2: see OmniSoc/Arduino_UART/SerialManager.h for
    // the canonical wire-format spec. Identical bytes on both sides.
    static constexpr uint8_t SYNC_0 = 0xA5;
    static constexpr uint8_t SYNC_1 = 0x5A;
    static constexpr int SYNC_SIZE = 2;
    static constexpr int HEADER_SIZE = 2;
    static constexpr int CRC_SIZE = 2;
    static constexpr int FLOAT_SIZE = 4;
    std::atomic<bool> timeoutFlag{true};
    std::atomic<bool> seekingFlag{true};
    std::chrono::steady_clock::time_point lastTimeoutClock;

    std::chrono::steady_clock::time_point asyncFlushClock;

    long byteSpacingTime_us = -1;

    // CRC-16/CCITT-FALSE — poly 0x1021, init 0xFFFF, no reflection, no xorout.
    // crc16_ccitt("123456789", 9) == 0x29B1.
    static uint16_t crc16_ccitt(const uint8_t* data, int len);
};

#endif // UART_SERIAL_H
