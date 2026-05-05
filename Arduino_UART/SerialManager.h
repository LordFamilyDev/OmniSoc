#ifndef SerialManager_H
#define SerialManager_H

#include <Arduino.h>

// OmniSoc UART framing v3:
//   [0xA5][0x5A][hdr:1][len:1][bytes:0..MAX_PAYLOAD][crc16_lo][crc16_hi]
//
// Sync bytes are an advisory pre-filter for parser scan — payload bytes CAN
// equal those values; CRC-16 (CCITT-FALSE, poly 0x1021, init 0xFFFF, no
// reflection, no xorout) is the actual frame-validity authority. CRC is
// computed over [hdr, len, bytes] only — NOT over the sync bytes.
//
// Payload is opaque bytes. Use PackBytes.h helpers (pack_u32 / pack_float /
// unpack_*) to build/parse typed fields. The float-array convenience
// overloads of sendMessage/receiveMessage exist for code that just wants a
// homogeneous float array.
//
// Total frame size: 6 + len bytes (max 6 + 48 = 54, comfortably under the
// AVR 64-byte hardware UART RX buffer).
//
// CALL-FREQUENCY CONTRACT: receiveMessage() is the ONLY thing draining the
// HardwareSerial RX buffer in this design — there is no background sync
// thread on Arduino. Caller MUST call receiveMessage() every main loop
// iteration (at minimum every ~10 ms at 57600 baud, where the 64 B HW buffer
// fills). Skipping calls causes silent byte loss and cascading CRC failures.

class SerialManager {
private:
    HardwareSerial* serial;
    static const uint8_t SYNC_0 = 0xA5;
    static const uint8_t SYNC_1 = 0x5A;
    static const int SYNC_SIZE = 2;
    static const int HEADER_SIZE = 1;
    static const int LEN_SIZE = 1;
    static const int CRC_SIZE = 2;
    static const int FRAME_OVERHEAD = SYNC_SIZE + HEADER_SIZE + LEN_SIZE + CRC_SIZE;  // 6
    bool timeoutFlag = true;
    long lastTimeoutClock = 0;
    long timeoutPeriod_ms;

    long byteSpacingTime_us = -1;

    // Internal accumulation buffer for sync-scan resync.
    // Holds 2+ max-size frames (max frame = 6 + MAX_PAYLOAD = 54 B).
    static const int RX_BUF_SIZE = 128;
    uint8_t rxBuf[RX_BUF_SIZE];
    int rxBufLen = 0;

    // Position in rxBuf from which to resume scanning for the sync pair.
    // Advanced past false syncs without memmoving the buffer; the buffer is
    // only compacted on valid frame extraction or when scan_pos exceeds the
    // compaction threshold. Drops resync from O(N²) to O(N).
    int scan_pos = 0;

public:

    static const uint8_t MAX_PAYLOAD = 48;          // bytes per frame payload (v3)
    static const uint8_t MAX_FLOATS  = MAX_PAYLOAD / 4;  // 12

    SerialManager(HardwareSerial& serialPort, int _timeoutPeriod_ms) : serial(&serialPort), timeoutPeriod_ms(_timeoutPeriod_ms) {}

    void connect(unsigned long baudRate)
    {
        byteSpacingTime_us = static_cast<long>(ceil(10000000.0 / baudRate));
        lastTimeoutClock = millis();
        serial->begin(baudRate);
    }
    void disconnect() { serial->end(); }
    bool isConnected() { return !timeoutFlag; }

    // User-callable reset: drops the internal scan buffer and the kernel
    // UART input buffer, resets scan position. Use after mode switches or
    // when the application detects prolonged corruption and wants to start
    // fresh. Not used for automatic resync — CRC-16 handles that.
    void flushIncomingSerial();

    // Bytes-primary API (v3).
    // sendMessage: returns 1 on success, -1 on failure (len > MAX_PAYLOAD).
    int sendMessage(uint8_t header, const uint8_t* bytes, uint8_t len);
    // receiveMessage: caller passes a MAX_PAYLOAD-sized buffer for `bytes`.
    // Returns 1 on a valid frame, negative on no-frame / partial / bad frame.
    // MUST be called every main loop iteration (see contract above).
    int receiveMessage(uint8_t& header, uint8_t* bytes, uint8_t& len);

    // Float-array convenience overloads. Wire format is identical — these just
    // pack/unpack floats into the byte payload internally.
    int sendMessage(uint8_t header, const float* data, uint8_t numFloats);
    int receiveMessage(uint8_t& header, float* data, uint8_t& numFloats);

private:
    // CRC-16/CCITT-FALSE — poly 0x1021, init 0xFFFF, no reflection, no xorout.
    // Test vector: crc16_ccitt("123456789", 9) == 0x29B1.
    static uint16_t crc16_ccitt(const uint8_t* data, int len) {
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < len; i++) {
            crc ^= ((uint16_t)data[i]) << 8;
            for (int j = 0; j < 8; j++) {
                if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
                else              crc = (uint16_t)(crc << 1);
            }
        }
        return crc;
    }
};

#endif // SerialManager_H
