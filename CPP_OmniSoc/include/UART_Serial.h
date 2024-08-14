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

    boost::asio::io_service io_service_;
    boost::asio::serial_port serial_;
    std::string port_;
    unsigned int baud_rate_;
    int timeoutPeriod_ms_;

    std::vector<char> buffer_;
    std::mutex buffer_mutex_;
    std::atomic<bool> running_;
    std::thread read_thread_;
    std::thread sync_thread_;

    static constexpr int HEADER_SIZE = 2;
    static constexpr int CHECKSUM_SIZE = 1;
    static constexpr int FLOAT_SIZE = 4;
    bool timeoutFlag = true;
    bool seekingFlag = true;
    std::chrono::steady_clock::time_point lastTimeoutClock;

    int lastHeader = -1;
    int lastNumFloats = 0;

    std::chrono::steady_clock::time_point asyncFlushClock;

    long byteSpacingTime_us = -1;

    uint8_t computeChecksum(const uint8_t* data, int size);
};

#endif // UART_SERIAL_H
