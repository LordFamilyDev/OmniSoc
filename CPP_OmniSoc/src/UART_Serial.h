#ifndef UART_SERIAL_H
#define UART_SERIAL_H

#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <chrono>

class UART_Serial {
public:
    UART_Serial(const std::string& port, unsigned int baud_rate, int timeoutPeriod_ms);
    ~UART_Serial();

    void connect();
    void disconnect();
    bool isConnected();

    void flushIncomingSerial();
    bool asyncFlushIncomingSerial();
    void handleSynchronization();

    int sendMessage(int header, const std::vector<float>& data);
    int receiveMessage(int& header, std::vector<float>& data);

private:
    boost::asio::io_service io_service_;
    boost::asio::serial_port serial_;
    boost::asio::streambuf buffer_;
    std::string port_;
    unsigned int baud_rate_;
    int timeoutPeriod_ms_;

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
