#include "Socket_Serial.h"

#include <chrono>
#include <iostream>

Socket_Serial::Socket_Serial(const std::string& _IP_Address, const std::string& _port, bool _isServer, bool _asyncronousFlag)
    : io_context_(), socket_(io_context_) {
    
    asyncronousFlag = _asyncronousFlag;
    IP_Address = _IP_Address;
    port = _port;
    isServer = _isServer;
}

void Socket_Serial::synchronousUpdate()
{
    if (asyncronousFlag) { return; } //no need to call syncronousUpdate in async mode
    doConnection();
    doSerial();
}

Socket_Serial::~Socket_Serial() {
    disconnect();
}

void Socket_Serial::connect(bool blocking_flag, bool auto_reconnect, int _period_ms) {
    if (!asyncronousFlag) { return; }

    period_ms = _period_ms;
    autoReconnect = auto_reconnect;

    connection_thread_ = std::thread(&Socket_Serial::connectionThread, this);

    if (blocking_flag)
    {
        while (!connectedFlag)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void Socket_Serial::disconnect() {
    try
    {
        if (acceptor_ != nullptr && acceptor_->is_open())
        { acceptor_->close(); }
    }
    catch(...){}

    try
    {
        std::cout << "Connection Closed" << std::endl;
        autoReconnect = false;
        killFlag = true;

        if (connection_thread_.joinable())
        { connection_thread_.join(); }

        if (serial_thread_.joinable())
        { serial_thread_.join(); }

        closeSocket();
    }
    catch (const std::exception& e) {
        if (!suppressCatchPrints) { std::cerr << "Exception caught: " << e.what() << std::endl; }
    }
    catch (...) {
        if (!suppressCatchPrints) { std::cerr << "Unknown exception caught." << std::endl; }
    }
}

void Socket_Serial::send(const std::string& msg) {
    std::lock_guard<std::mutex> lock(out_buffer_mutex_);
    outgoing_buffer_.push_back(msg);
}

std::vector<std::string> Socket_Serial::receive(int count) {
    std::vector<std::string> messages;
    std::unique_lock<std::mutex> lock(in_buffer_mutex_);

    if (count == -1) {
        messages = std::move(incoming_buffer_);
        incoming_buffer_.clear();
    }
    else {
        int num_messages = std::min(count, static_cast<int>(incoming_buffer_.size()));
        messages.insert(messages.end(), incoming_buffer_.begin(), incoming_buffer_.begin() + num_messages);
        incoming_buffer_.erase(incoming_buffer_.begin(), incoming_buffer_.begin() + num_messages);
    }

    return messages;
}

bool Socket_Serial::isConnected() {
    return connectedFlag;
}

void Socket_Serial::clearInBuffer() {
    std::lock_guard<std::mutex> lock(in_buffer_mutex_);
    incoming_buffer_.clear();
}

void Socket_Serial::clearOutBuffer() {
    std::lock_guard<std::mutex> lock(out_buffer_mutex_);
    outgoing_buffer_.clear();
}

void Socket_Serial::flushSocket() {
    // No need to flush for TCP/IP sockets
}

void Socket_Serial::connectionThread() {
    if (!asyncronousFlag) { return; }

    bool connectionOneShot = true;

    while (!killFlag && (autoReconnect || connectionOneShot)) {
        if (!connectedFlag)
        {
            if (serial_thread_.joinable())
            { serial_thread_.join(); }

            doConnection();

            if (socket_.is_open()) 
            {
                serial_thread_ = std::thread(&Socket_Serial::serialThread, this);
                connectionOneShot = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Socket_Serial::doConnection()
{
    try {
        if (!connectedFlag) {

            boost::asio::ip::tcp::resolver resolver(io_context_);
            //boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), IP_Address, port);
            //boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
            auto endpoints = resolver.resolve(IP_Address, port);

            if (isServer) {
                acceptor_ = std::make_shared < boost::asio::ip::tcp::acceptor>(io_context_, *endpoints.begin());
                bool acceptError = false;
                acceptor_->accept(socket_);
            }
            else {
                boost::asio::connect(socket_, endpoints);
            }

            if (socket_.is_open()) {
                socket_.non_blocking(true);
                std::cout << "socket connected" << std::endl;
                missedHeartbeats = -5; //slight grace period for initial connection.
                connectedFlag = true;
            }
        }
    }
    catch (const std::exception& e) {
        // Catch and print standard exceptions
        if (!suppressCatchPrints) { std::cout << "Connection Exception: " << e.what() << std::endl; }
        //closeSocket();
    }
    catch (...) {
        // Catch and print any other exception
        if (!suppressCatchPrints) { std::cout << "Unknown exception caught in connection." << std::endl; }
    }
}

void Socket_Serial::closeSocket()
{
    if (connectedFlag)
    {
        std::cout << "Connection Dropped" << std::endl;
    }

    boost::system::error_code ec;

    if(socket_.is_open())
    {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && !suppressCatchPrints) {
            std::cerr << "Failed to shutdown socket: " << ec.message() << std::endl;
        }
        socket_.close(ec);
        if (ec && !suppressCatchPrints) {
            std::cerr << "Failed to close socket: " << ec.message() << std::endl;
        }
        socket_.release(ec);
        if (ec && !suppressCatchPrints) {
            std::cerr << "Failed to release socket: " << ec.message() << std::endl;
        }
    }

    if(acceptor_ != nullptr)
    {
        acceptor_->close(ec);
        if (ec && !suppressCatchPrints) {
            std::cerr << "Failed to close acceptor: " << ec.message() << std::endl;
        }

        acceptor_->release(ec);
        if (ec && !suppressCatchPrints) {
            std::cerr << "Failed to release acceptor: " << ec.message() << std::endl;
        }
    }

    connectedFlag = false;
}

void Socket_Serial::serialThread() {
    if (!asyncronousFlag) { return; }

    while (!killFlag && connectedFlag) {
        doSerial();
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
}

void Socket_Serial::doSerial()
{
    sendMessages();
    readMessages();
}

void Socket_Serial::sendMessages() {
    if (!connectedFlag) { return; }

    std::lock_guard<std::mutex> lock(out_buffer_mutex_);

    try
    {
        if (!outgoing_buffer_.empty()) {
            for (const auto& msg : outgoing_buffer_) {
                boost::asio::write(socket_, boost::asio::buffer(msg));
                boost::asio::write(socket_, boost::asio::buffer(msgDelimiter));
            }
            outgoing_buffer_.clear();
        }
        else {
            boost::asio::write(socket_, boost::asio::buffer(msgDelimiter));
        }
    }
    catch (...) {
        if (!suppressCatchPrints) { std::cerr << "Write error: " << std::endl; }
        closeSocket();
    }
}

void Socket_Serial::readMessages() {
    if (!connectedFlag) { return; }

    try
    {
        char buffer[1024];
        boost::system::error_code error;
        size_t bytes_read = socket_.read_some(boost::asio::buffer(buffer), error);

        if (error == boost::asio::error::eof || error == boost::asio::error::would_block) {
            missedHeartbeats++;
            if (missedHeartbeats >= missedHeartbeatLimit) {
                std::cout << "Heartbeat kill: " << missedHeartbeats << std::endl;
                closeSocket();
            }
        }
        else if (!error) {
            if (bytes_read > 0) {
                std::string message(buffer, bytes_read);
                std::string tempRemainder;
                std::vector<std::string> msgs = splitMessage(message, msgDelimiter, tempRemainder);
                for (int i = 0; i < msgs.size(); i++)
                {
                    std::string isolatedMessage = "";
                    if (i == 0 && inMessageRemainder.size()>0)
                    { isolatedMessage = inMessageRemainder + msgs[i]; }
                    else
                    { isolatedMessage = msgs[i]; }

                    if (isolatedMessage.size() > 0)
                    {
                        std::lock_guard<std::mutex> lock(in_buffer_mutex_);
                        incoming_buffer_.emplace_back(isolatedMessage);
                    }
                }
                if (msgs.size() > 0)
                { inMessageRemainder = tempRemainder; }
                else
                { inMessageRemainder = inMessageRemainder + tempRemainder; }
                
                missedHeartbeats = 0;
            }
        }
        else
        {
            closeSocket();
        }
    }
    catch (...) {
        if (!suppressCatchPrints) { std::cerr << "Read error: " << std::endl; }
        closeSocket();
    }
}

std::vector<std::string> Socket_Serial::splitMessage(const std::string& message, const std::string& delimiter) {
    std::string inMsg = message;
    std::vector<std::string> messages;
    size_t pos = 0;
    std::string token;

    while ((pos = inMsg.find(delimiter)) != std::string::npos) {
        token = inMsg.substr(0, pos);
        messages.push_back(token);
        inMsg.erase(0, pos + delimiter.length());
    }

    if (!inMsg.empty()) {
        messages.push_back(inMsg);
    }

    return messages;
}