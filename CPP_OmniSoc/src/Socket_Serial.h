#ifndef SERIAL_MANAGER_HPP
#define SERIAL_MANAGER_HPP

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
    std::thread connection_thread_;
    std::thread serial_thread_;
    std::mutex in_buffer_mutex_;
    std::mutex out_buffer_mutex_;
    std::vector<std::string> incoming_buffer_;
    std::vector<std::string> outgoing_buffer_;
    std::condition_variable in_buffer_cv_;
    bool autoReconnect = false;
    int missedHeartbeats = 0;
    int missedHeartbeatLimit = 3;
    bool isServer = false;
    bool connectedFlag = false;
    bool killFlag = false;
    std::string heartBeat = ";";

    std::string IP_Address;
    std::string port;
    int period_ms;

public:
    Socket_Serial(const std::string& _IP_Address, const std::string& _port, bool _isServer);
    ~Socket_Serial();

    void connect(bool blocking_flag, bool auto_reconnect, int _period_ms);
    void disconnect();
    void send(const std::string& msg, bool send_now_flag = false);
    std::vector<std::string> receive(int count = -1, bool wait_flag = false);
    bool isConnected();
    void clearInBuffer();
    void clearOutBuffer();
    void flushSocket();

private:
    void connectionThread();
    void serialThread();
    void sendMessages();
    void readMessages();
};

#endif // SERIAL_MANAGER_HPP