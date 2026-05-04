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
    uint8_t messageSize = SYNC_SIZE + HEADER_SIZE + 1 + numFloats * FLOAT_SIZE + CRC_SIZE;
    uint8_t message[messageSize];

    // Sync bytes (advisory pre-filter for the receiver — not in the CRC).
    message[0] = SYNC_0;
    message[1] = SYNC_1;

    // Header (big-endian).
    message[SYNC_SIZE + 0] = (header >> 8) & 0xFF;
    message[SYNC_SIZE + 1] = header & 0xFF;

    // numFloats.
    message[SYNC_SIZE + HEADER_SIZE] = numFloats;

    // Float payload.
    for (int i = 0; i < numFloats; i++) {
        memcpy(&message[SYNC_SIZE + HEADER_SIZE + 1 + i * FLOAT_SIZE],
               &data[i], FLOAT_SIZE);
    }

    // CRC-16 over [hdr_hi, hdr_lo, numFloats, float_bytes] — sync excluded.
    uint16_t crc = crc16_ccitt(&message[SYNC_SIZE],
                               HEADER_SIZE + 1 + numFloats * FLOAT_SIZE);
    message[messageSize - 2] = (uint8_t)(crc & 0xFF);          // little-endian
    message[messageSize - 1] = (uint8_t)((crc >> 8) & 0xFF);

    serial->write(message, messageSize);
    return 1;
}

int SerialManager::receiveMessage(int& header, float* data, int& numFloats)
{
    // Expire the connection if we haven't parsed a good frame in a while.
    if (!timeoutFlag && !seekingFlag &&
        millis() - lastTimeoutClock > timeoutPeriod_ms)
    { timeoutFlag = true; }

    // Startup / manual-flush resync uses the quiet-gap detector in
    // handleSynchronization(). During that window, don't touch the stream.
    if (seekingFlag) { return -5; }

    // Drain whatever UART has into our internal buffer (bounded).
    while (serial->available() > 0 && rxBufLen < RX_BUF_SIZE)
    { rxBuf[rxBufLen++] = (uint8_t)serial->read(); }

    int lastStatus = -1;

    // Sync-scan loop. On any per-frame failure we drop the leading sync (or 1
    // junk byte if no sync was found yet) and rescan.
    while (rxBufLen >= SYNC_SIZE)
    {
        // Find the next 0xA5 0x5A in rxBuf.
        int syncIdx = -1;
        for (int i = 0; i + 1 < rxBufLen; i++) {
            if (rxBuf[i] == SYNC_0 && rxBuf[i + 1] == SYNC_1) {
                syncIdx = i;
                break;
            }
        }

        if (syncIdx < 0) {
            // No sync anywhere. Drop everything except the trailing byte (it
            // might be a 0xA5 starting a sync that hasn't completed yet).
            if (rxBufLen > 1) {
                rxBuf[0] = rxBuf[rxBufLen - 1];
                rxBufLen = 1;
            }
            return lastStatus;
        }

        // Drop pre-sync junk.
        if (syncIdx > 0) {
            int remaining = rxBufLen - syncIdx;
            memmove(rxBuf, rxBuf + syncIdx, remaining);
            rxBufLen = remaining;
        }

        // Need at least sync + header + count = 5 bytes to look at numFloats.
        if (rxBufLen < SYNC_SIZE + HEADER_SIZE + 1) {
            return -1;
        }

        int hdr = (rxBuf[SYNC_SIZE] << 8) | rxBuf[SYNC_SIZE + 1];
        int nf  = rxBuf[SYNC_SIZE + HEADER_SIZE];

        if (nf < 0 || nf > maxFloats) {
            // Implausible numFloats — false sync. Drop the sync bytes only;
            // a real sync may follow shortly.
            memmove(rxBuf, rxBuf + SYNC_SIZE, rxBufLen - SYNC_SIZE);
            rxBufLen -= SYNC_SIZE;
            lastStatus = -4;
            continue;
        }

        int total = SYNC_SIZE + HEADER_SIZE + 1 + nf * FLOAT_SIZE + CRC_SIZE;
        if (rxBufLen < total) {
            // Frame not fully arrived yet. Come back next call.
            return -2;
        }

        // CRC check covers [hdr_hi, hdr_lo, numFloats, float_bytes] — sync excluded.
        uint16_t computed = crc16_ccitt(&rxBuf[SYNC_SIZE],
                                         HEADER_SIZE + 1 + nf * FLOAT_SIZE);
        uint16_t received = (uint16_t)rxBuf[total - 2]
                          | ((uint16_t)rxBuf[total - 1] << 8);

        if (computed != received) {
            // False sync match (junk byte happened to be 0xA5 0x5A) or real
            // frame got corrupted. Drop the sync; rescan finds the next.
            memmove(rxBuf, rxBuf + SYNC_SIZE, rxBufLen - SYNC_SIZE);
            rxBufLen -= SYNC_SIZE;
            lastStatus = -3;
            continue;
        }

        // Valid frame. Extract.
        header    = hdr;
        numFloats = nf;
        for (int i = 0; i < nf; i++) {
            memcpy(&data[i],
                   &rxBuf[SYNC_SIZE + HEADER_SIZE + 1 + i * FLOAT_SIZE],
                   FLOAT_SIZE);
        }

        int remaining = rxBufLen - total;
        if (remaining > 0) memmove(rxBuf, rxBuf + total, remaining);
        rxBufLen = remaining;

        timeoutFlag = false;
        lastTimeoutClock = millis();
        return 1;
    }

    return lastStatus;
}
