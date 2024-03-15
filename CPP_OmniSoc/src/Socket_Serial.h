#ifndef SOCKET_SERIAL_H
#define SOCKET_SERIAL_H

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

class Socket_Serial {
private:
    boost::asio::io_service io_service_;
    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::thread connection_thread_;
    std::thread serial_thread_;
    std::mutex in_buffer_mutex_;
    std::mutex out_buffer_mutex_;
    std::vector<std::string> incoming_buffer_;
    std::vector<std::string> outgoing_buffer_;

    bool asyncronousFlag = false;
    bool autoReconnect = false;
    int missedHeartbeats = 0;
    int missedHeartbeatLimit = 3;
    bool isServer = false;
    bool connectedFlag = false;
    bool killFlag = false;
    std::string msgDelimiter = ";";
    std::string inMessageRemainder = "";

    std::string IP_Address;
    std::string port;
    int period_ms;

public:
    Socket_Serial(const std::string& _IP_Address, const std::string& _port, bool _isServer, bool _asyncronousFlag = true);
    ~Socket_Serial();

    void connect(bool blocking_flag, bool auto_reconnect, int _period_ms);
    void disconnect();
    void send(const std::string& msg);
    std::vector<std::string> receive(int count = -1);

    bool isConnected();
    void clearInBuffer();
    void clearOutBuffer();
    void flushSocket();

    /// <summary>
    /// Allows synchronous operation (primarily for debug)
    /// </summary>
    void synchronousUpdate();

    static std::vector<std::string> Socket_Serial::splitMessage(const std::string& message, const std::string& delimiter, std::string& remainder);

    bool suppressCatchPrints = true;

private:
    void connectionThread();
    void serialThread();

    void doConnection();
    void doSerial();

    void sendMessages();
    void readMessages();


    void closeSocket();
};

#endif // SOCKET_SERIAL_H