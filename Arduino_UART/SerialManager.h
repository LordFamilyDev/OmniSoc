#ifndef SerialManager_H
#define SerialManager_H

#include <Arduino.h>
class SerialManager {
private:
    HardwareSerial* serial;
    const int HEADER_SIZE = 2;
    const int CHECKSUM_SIZE = 1;
    const int FLOAT_SIZE = 4;
    bool timeoutFlag = true;
    bool seekingFlag = true;
    long lastTimeoutClock = 0;
    long timeoutPeriod_ms;

    int lastHeader = -1;
    int lastNumFloats = 0;

    long asyncFlushClock = 0;

    long byteSpacingTime_us = -1;

    

public:

    //arduino has a max serial buffer size of 64 bytes (technically this could be 14 bytes)
    static const int maxFloats = 10;
    
    SerialManager(HardwareSerial& serialPort, int _timeoutPeriod_ms) : serial(&serialPort),timeoutPeriod_ms(_timeoutPeriod_ms)  {}

    void connect(unsigned long baudRate) 
    { 
        byteSpacingTime_us = static_cast<long>(ceil(10000000.0 / baudRate));
        Serial.println(byteSpacingTime_us);
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
    uint8_t computeChecksum(const uint8_t* data, int size) {
        uint8_t checksum = 0;
        for (int i = 0; i < size; i++) {
            checksum ^= data[i];
        }
        return checksum;
    }
};

#endif // SERIAL_MANAGER_H