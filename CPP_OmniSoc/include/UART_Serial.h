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
    UART_Serial(const std::string& port, unsigned int baud_rate, int timeoutPeriod_ms,
                bool tx_pacing_enabled = true);
    ~UART_Serial();

    void connect();
    void disconnect();
    bool isConnected();
    size_t available();

    // User-callable reset: drops the internal scan buffer and the kernel
    // UART input buffer, resets scan position. Use after mode switches or
    // when the application detects prolonged corruption and wants to start
    // fresh. Not used for automatic resync — CRC-16 handles that.
    void flushIncomingSerial();

    // Bytes-primary API (v3).
    // sendMessage: returns 1 on success, -1 on failure (len > MAX_PAYLOAD or write error).
    int sendMessage(uint8_t header, const uint8_t* bytes, uint8_t len);
    // receiveMessage: caller passes a MAX_PAYLOAD-sized buffer for `bytes`.
    // Returns 1 on a valid frame, negative on no-frame / partial / bad frame.
    int receiveMessage(uint8_t& header, uint8_t* bytes, uint8_t& len);

    // Float-array convenience overloads. Wire format is identical — these just
    // pack/unpack floats into the byte payload internally.
    int sendMessage(uint8_t header, const float* data, uint8_t numFloats);
    // receiveMessage (floats): caller passes a buffer sized for MAX_PAYLOAD/4 floats.
    // Returns -6 if the received payload length is not a multiple of 4 bytes.
    int receiveMessage(uint8_t& header, float* data, uint8_t& numFloats);

    // Diagnostic: count of bytes dropped due to internal buffer cap overflow.
    size_t getDroppedBytesCount() const { return dropped_bytes_.load(); }

    static constexpr uint8_t MAX_PAYLOAD = 48;          // v3 max payload bytes per frame
    static constexpr uint8_t MAX_FLOATS  = MAX_PAYLOAD / 4;  // 12

private:
    void readFromSerial();
    void startWorkThreads();
    void stopWorkThreads();

    boost::asio::io_context io_context_;
    boost::asio::serial_port serial_;
    std::string port_;
    unsigned int baud_rate_;
    int timeoutPeriod_ms_;

    std::vector<uint8_t> buffer_;
    std::mutex buffer_mutex_;
    std::atomic<bool> running_;
    std::thread read_thread_;

    // TX self-pacing. sendMessage() waits until earliest_next_send_ before
    // dispatching, then advances earliest_next_send_ by the new frame's
    // wire time. This caps the on-wire byte rate at the configured baud
    // rate, so a sender can fire-and-forget back-to-back frames without
    // overflowing a slow receiver's HW UART buffer (Arduino is 64 B).
    // Mutex also serializes concurrent senders on a shared serial_port —
    // boost::asio doesn't guarantee that on its own.
    std::mutex send_mutex_;
    std::chrono::steady_clock::time_point earliest_next_send_;
    bool tx_pacing_enabled_;

    // Position in buffer_ from which the parser should resume scanning for
    // the sync byte pair. Advanced past false syncs without compacting; the
    // buffer is only erased on valid frame extraction or when scan_pos_
    // exceeds the compaction threshold. Drops resync from O(N²) to O(N).
    size_t scan_pos_ = 0;

    // OmniSoc UART framing v3: see OmniSoc/Arduino_UART/SerialManager.h for
    // the canonical wire-format spec. Identical bytes on both sides.
    //   [0xA5][0x5A][hdr:1][len:1][bytes:0..MAX_PAYLOAD][crc16_lo][crc16_hi]
    // CRC-16 covers [hdr][len][bytes] only — sync excluded.
    static constexpr uint8_t SYNC_0 = 0xA5;
    static constexpr uint8_t SYNC_1 = 0x5A;
    static constexpr int SYNC_SIZE = 2;
    static constexpr int HEADER_SIZE = 1;
    static constexpr int LEN_SIZE = 1;
    static constexpr int CRC_SIZE = 2;
    static constexpr int FRAME_OVERHEAD = SYNC_SIZE + HEADER_SIZE + LEN_SIZE + CRC_SIZE;  // 6
    static constexpr int MAX_FRAME_SIZE = FRAME_OVERHEAD + MAX_PAYLOAD;                    // 54

    // Internal buffer cap (~4× max frame). On overflow in readFromSerial(),
    // drop the oldest half and bump dropped_bytes_.
    static constexpr size_t BUFFER_CAP = 256;

    std::atomic<bool> timeoutFlag{true};
    std::chrono::steady_clock::time_point lastTimeoutClock;

    long byteSpacingTime_us = -1;

    std::atomic<size_t> dropped_bytes_{0};

    // CRC-16/CCITT-FALSE — poly 0x1021, init 0xFFFF, no reflection, no xorout.
    // crc16_ccitt("123456789", 9) == 0x29B1.
    static uint16_t crc16_ccitt(const uint8_t* data, int len);
};

#endif // UART_SERIAL_H
