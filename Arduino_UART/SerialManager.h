#ifndef SerialManager_H
#define SerialManager_H

#include <Arduino.h>

// OmniSoc UART framing v2:
//   [0xA5][0x5A][hdr_hi][hdr_lo][numFloats][float bytes][crc16_lo][crc16_hi]
//
// Sync bytes are an advisory pre-filter for parser scan — float payload
// CAN contain those byte values; CRC-16 (CCITT-FALSE, poly 0x1021, init
// 0xFFFF, no reflection, no xorout) is the actual frame-validity authority.
// CRC is computed over [hdr_hi, hdr_lo, numFloats, float_bytes] only —
// NOT over the sync bytes.
//
// Total frame size: 7 + 4*numFloats bytes.

class SerialManager {
private:
    HardwareSerial* serial;
    static const uint8_t SYNC_0 = 0xA5;
    static const uint8_t SYNC_1 = 0x5A;
    static const int SYNC_SIZE = 2;
    static const int HEADER_SIZE = 2;
    static const int CRC_SIZE = 2;
    static const int FLOAT_SIZE = 4;
    bool timeoutFlag = true;
    bool seekingFlag = true;
    long lastTimeoutClock = 0;
    long timeoutPeriod_ms;

    long asyncFlushClock = 0;

    long byteSpacingTime_us = -1;

    // Internal accumulation buffer for sync-scan resync.
    // Holds 2+ max-size frames (max frame = 7 + 10*4 = 47 B at maxFloats=10).
    static const int RX_BUF_SIZE = 128;
    uint8_t rxBuf[RX_BUF_SIZE];
    int rxBufLen = 0;

public:

    //arduino has a max serial buffer size of 64 bytes (technically this could be 14 bytes)
    static const int maxFloats = 10;

    SerialManager(HardwareSerial& serialPort, int _timeoutPeriod_ms) : serial(&serialPort),timeoutPeriod_ms(_timeoutPeriod_ms)  {}

    void connect(unsigned long baudRate)
    {
        byteSpacingTime_us = static_cast<long>(ceil(10000000.0 / baudRate));
        asyncFlushClock = micros();
        lastTimeoutClock = millis();
        serial->begin(baudRate);
    }
    void disconnect() { serial->end(); }
    bool isConnected() { return !timeoutFlag; }

    //attempts to clear until the end of a message (blocking).
    void flushIncomingSerial();

    //flush asyncronously through rapid calling (every main loop) until returns true
    bool asyncFlushIncomingSerial();

    //Needs to be called every loop to handle desync and resync
    void handleSynchronization()
    {
        if(seekingFlag)
        {
            bool flushResult = asyncFlushIncomingSerial();
            seekingFlag = !flushResult;
            return;
        }
    }

    int sendMessage(int header, float* data, int numFloats);

    //Needs to be called every loop if intended to handle multi-frequency messages.
    //Can be called at set interval for static frequency coms
    int receiveMessage(int& header, float* data, int& numFloats);

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
