#include "SerialManager.h"

#include <string.h>

void SerialManager::flushIncomingSerial()
{
    long tempTime = micros();
    while(micros() - tempTime < byteSpacingTime_us * 2)
    {
        if(serial->available())
        {
            serial->read();
            tempTime = micros();
        }
    }
}

bool SerialManager::asyncFlushIncomingSerial()
{
    while(serial->available())
    {
        serial->read();
        asyncFlushClock = micros();
    }

    if(micros() - asyncFlushClock > byteSpacingTime_us * 2)
    { return true; }

    return false;
}

int SerialManager::sendMessage(int header, float* data, int numFloats) 
{
    uint8_t messageSize = HEADER_SIZE + 1 + numFloats * FLOAT_SIZE + CHECKSUM_SIZE;
    uint8_t message[messageSize];

    // Add header to the message
    message[0] = (header >> 8) & 0xFF;
    message[1] = header & 0xFF;

    // Add number of floats to the message
    message[2] = numFloats;

    // Add float data to the message
    for (int i = 0; i < numFloats; i++) {
        uint8_t* floatBytes = reinterpret_cast<uint8_t*>(&data[i]);
        for (int j = 0; j < FLOAT_SIZE; j++) {
            message[HEADER_SIZE + 1 + i * FLOAT_SIZE + j] = floatBytes[j];
        }
    }

    // Compute and add checksum to the message
    uint8_t checksum = computeChecksum(message, messageSize - CHECKSUM_SIZE);
    message[messageSize - 1] = checksum;

    serial->write(message, messageSize);

    return 1;
}

int SerialManager::receiveMessage(int& header, float* data, int& numFloats)
{
    // Expire the connection if we haven't parsed a good frame in a while.
    if (!timeoutFlag && !seekingFlag &&
        millis() - lastTimeoutClock > timeoutPeriod_ms)
    { timeoutFlag = true; }

    // Startup / manual-flush resync still uses the quiet-gap detector in
    // handleSynchronization(). During that window, don't touch the stream.
    if (seekingFlag) { return -5; }

    // Drain whatever UART has into our internal buffer (bounded).
    while (serial->available() > 0 && rxBufLen < RX_BUF_SIZE)
    { rxBuf[rxBufLen++] = (uint8_t)serial->read(); }

    // Parse from the head of the buffer. On any parse failure we drop exactly
    // one byte and retry the new alignment — a single bad byte costs a single
    // frame, not the whole pipeline of queued good frames.
    int lastStatus = -1; // default: not enough bytes for even a header
    while (rxBufLen >= HEADER_SIZE + 1)
    {
        int hdr = (rxBuf[0] << 8) | rxBuf[1];
        int nf  = rxBuf[2];

        if (nf < 0 || nf > maxFloats)
        {
            // Implausible numFloats — can't be the start of a real frame.
            memmove(rxBuf, rxBuf + 1, --rxBufLen);
            lastStatus = -4;
            continue;
        }

        int total = HEADER_SIZE + 1 + nf * FLOAT_SIZE + CHECKSUM_SIZE;
        if (rxBufLen < total)
        {
            // Header looks plausible but frame isn't fully in yet; come back
            // next call with more bytes.
            return -2;
        }

        uint8_t chk = computeChecksum(rxBuf, total - CHECKSUM_SIZE);
        if (chk != rxBuf[total - 1])
        {
            // Bad checksum — drop one byte and try the next alignment. We do
            // NOT drain the rest of the buffer: the byte after this bad frame
            // is very likely the start of a good one.
            memmove(rxBuf, rxBuf + 1, --rxBufLen);
            lastStatus = -3;
            continue;
        }

        // Valid frame. Extract floats and consume `total` bytes from the buffer.
        header    = hdr;
        numFloats = nf;
        for (int i = 0; i < nf; i++)
        { memcpy(&data[i], &rxBuf[HEADER_SIZE + 1 + i * FLOAT_SIZE], FLOAT_SIZE); }

        int remaining = rxBufLen - total;
        if (remaining > 0) memmove(rxBuf, rxBuf + total, remaining);
        rxBufLen = remaining;

        timeoutFlag = false;
        lastTimeoutClock = millis();
        return 1;
    }

    return lastStatus;
}