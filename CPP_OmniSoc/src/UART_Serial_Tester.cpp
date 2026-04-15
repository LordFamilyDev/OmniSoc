#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include "UART_Serial.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <glob.h>
#endif

// ── Port discovery ────────────────────────────────────────────────────────────

std::vector<std::string> listPorts()
{
    std::vector<std::string> ports;
#ifdef _WIN32
    for (int i = 1; i <= 256; ++i)
    {
        std::string name = "COM" + std::to_string(i);
        HANDLE h = CreateFileA(("\\\\.\\" + name).c_str(),
                               GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) { ports.push_back(name); CloseHandle(h); }
    }
#else
    const char* patterns[] = { "/dev/ttyUSB*", "/dev/ttyACM*" };
    for (const char* pattern : patterns)
    {
        glob_t g;
        if (glob(pattern, GLOB_NOSORT, nullptr, &g) == 0)
        {
            for (size_t i = 0; i < g.gl_pathc; ++i)
                ports.push_back(g.gl_pathv[i]);
            globfree(&g);
        }
    }
#endif
    return ports;
}

// ── Background receive thread ─────────────────────────────────────────────────

struct ReceivedMsg
{
    int header = -1;
    std::vector<float> data;
    bool fresh = false; // true if not yet displayed
};

std::mutex            recvMutex;
ReceivedMsg           latestMsg;
std::atomic<bool>     killCommand(false);

void receiveThread_doWork(std::shared_ptr<UART_Serial> serial)
{
    while (!killCommand)
    {
        int header = 0;
        std::vector<float> data;
        if (serial->receiveMessage(header, data) == 1)
        {
            std::lock_guard<std::mutex> lock(recvMutex);
            latestMsg = { header, data, true };
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ── Input parsing ─────────────────────────────────────────────────────────────
// Expected format: <header_int>,<float>,<float>,...   e.g.  7,1.0,2.5,3.0

bool parseInput(const std::string& line, int& header, std::vector<float>& floats)
{
    std::istringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(ss, token, ','))
    {
        size_t start = token.find_first_not_of(" \t");
        size_t end   = token.find_last_not_of(" \t");
        if (start != std::string::npos)
            tokens.push_back(token.substr(start, end - start + 1));
    }

    if (tokens.empty()) return false;

    try
    {
        header = std::stoi(tokens[0]);
        floats.clear();
        for (size_t i = 1; i < tokens.size(); ++i)
            floats.push_back(std::stof(tokens[i]));
    }
    catch (...) { return false; }

    if (floats.size() > static_cast<size_t>(UART_Serial::maxFloats)) return false;

    return true;
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main()
{
    std::cout << "OmniSoc UART Tester  |  'x' to exit\n\n";

    // List available ports
    auto ports = listPorts();
    if (ports.empty())
    {
        std::cout << "No serial ports detected.\n";
    }
    else
    {
        std::cout << "Available ports:\n";
        for (auto& p : ports) std::cout << "  " << p << "\n";
    }

    std::cout << "\nChoose port: ";
    std::string port;
    std::cin >> port;
    std::cin.ignore();

    int baudRate   = 57600;
    int timeout_ms = 1000;

    auto serial = std::make_shared<UART_Serial>(port, baudRate, timeout_ms);
    serial->connect();

    std::cout << "\nConnected. Input format: <header>,<float>,<float>,...\n"
              << "Example: 7,1.0,2.5,3.0\n\n";

    std::thread recvThread(receiveThread_doWork, serial);

    while (!killCommand)
    {
        std::cout << "> ";
        std::string line;
        std::getline(std::cin, line);

        if (line == "x" || line == "X")
        {
            killCommand = true;
            break;
        }
        if (line.empty()) continue;

        int header;
        std::vector<float> floats;
        if (!parseInput(line, header, floats))
        {
            std::cout << "  [invalid] format: <header_int>,<float>,<float>,...\n";
            continue;
        }

        if (!serial->isConnected())
            std::cout << "  [not yet connected — sending anyway]\n";

        serial->sendMessage(header, floats);

        // Brief wait so a reply has a chance to arrive before we display
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Display latest received message
        ReceivedMsg msg;
        {
            std::lock_guard<std::mutex> lock(recvMutex);
            msg = latestMsg;
            latestMsg.fresh = false;
        }

        if (msg.header < 0)
        {
            std::cout << "  << (nothing received yet)\n";
        }
        else
        {
            std::cout << "  << " << msg.header;
            for (float f : msg.data) std::cout << ", " << f;
            if (!msg.fresh) std::cout << "  [stale]";
            std::cout << "\n";
        }
    }

    serial->disconnect();
    if (recvThread.joinable()) recvThread.join();

    return 0;
}
