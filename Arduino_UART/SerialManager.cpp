#include "SerialManager.h"

#include <string.h>

void SerialManager::flushIncomingSerial()
{
    while (serial->available()) serial->read();
    rxBufLen = 0;
    scan_pos = 0;
}

int SerialManager::sendMessage(uint8_t header, const uint8_t* bytes, uint8_t len)
{
    if (len > MAX_PAYLOAD) return -1;

    uint8_t messageSize = FRAME_OVERHEAD + len;
    uint8_t message[MAX_PAYLOAD + FRAME_OVERHEAD];

    // Sync bytes (advisory pre-filter for the receiver — not in the CRC).
    message[0] = SYNC_0;
    message[1] = SYNC_1;
    message[SYNC_SIZE] = header;
    message[SYNC_SIZE + HEADER_SIZE] = len;
    if (len > 0) {
        memcpy(&message[SYNC_SIZE + HEADER_SIZE + LEN_SIZE], bytes, len);
    }

    // CRC-16 over [hdr][len][bytes] — sync excluded.
    uint16_t crc = crc16_ccitt(&message[SYNC_SIZE], HEADER_SIZE + LEN_SIZE + len);
    message[messageSize - 2] = (uint8_t)(crc & 0xFF);          // little-endian
    message[messageSize - 1] = (uint8_t)((crc >> 8) & 0xFF);

    serial->write(message, messageSize);
    return 1;
}

int SerialManager::sendMessage(uint8_t header, const float* data, uint8_t numFloats)
{
    uint8_t lenBytes = numFloats * 4;
    if (lenBytes > MAX_PAYLOAD) return -1;
    uint8_t buf[MAX_PAYLOAD];
    if (numFloats > 0) memcpy(buf, data, lenBytes);
    return sendMessage(header, buf, lenBytes);
}

int SerialManager::receiveMessage(uint8_t& header, uint8_t* bytes, uint8_t& len)
{
    // Expire the connection if we haven't parsed a good frame in a while.
    if (!timeoutFlag &&
        millis() - lastTimeoutClock > timeoutPeriod_ms)
    { timeoutFlag = true; }

    // Drain whatever UART has into our internal buffer (bounded).
    while (serial->available() > 0 && rxBufLen < RX_BUF_SIZE)
    { rxBuf[rxBufLen++] = (uint8_t)serial->read(); }

    int lastStatus = -1;

    // Sync-scan loop with scan_pos offset — advance past false syncs without
    // memmoving the buffer. Compact only on valid frame extraction or when
    // scan_pos exceeds half the buffer length.
    while (true)
    {
        // Compact if scan_pos has crept too far forward.
        if (scan_pos > 0 && scan_pos > rxBufLen / 2) {
            int remaining = rxBufLen - scan_pos;
            if (remaining > 0) memmove(rxBuf, rxBuf + scan_pos, remaining);
            rxBufLen = remaining;
            scan_pos = 0;
        }

        if (rxBufLen < scan_pos + SYNC_SIZE) {
            return lastStatus;
        }

        // Find next 0xA5 0x5A starting at scan_pos.
        int syncIdx = -1;
        for (int i = scan_pos; i + 1 < rxBufLen; i++) {
            if (rxBuf[i] == SYNC_0 && rxBuf[i + 1] == SYNC_1) {
                syncIdx = i;
                break;
            }
        }

        if (syncIdx < 0) {
            // No sync. Preserve trailing byte (possible 0xA5 with 0x5A pending).
            if (rxBufLen > 1) {
                scan_pos = rxBufLen - 1;
            } else {
                scan_pos = 0;
            }
            return lastStatus;
        }

        // Need sync + header + len = 4 bytes minimum to look at len.
        if (rxBufLen < syncIdx + SYNC_SIZE + HEADER_SIZE + LEN_SIZE) {
            scan_pos = syncIdx;
            return -1;
        }

        uint8_t hdr = rxBuf[syncIdx + SYNC_SIZE];
        uint8_t plen = rxBuf[syncIdx + SYNC_SIZE + HEADER_SIZE];

        if (plen > MAX_PAYLOAD) {
            // Implausible len — false sync. Advance one byte and rescan.
            scan_pos = syncIdx + 1;
            lastStatus = -4;
            continue;
        }

        int total = syncIdx + FRAME_OVERHEAD + plen;
        if (rxBufLen < total) {
            // Frame not fully arrived yet. Stay parked at this sync.
            scan_pos = syncIdx;
            return -2;
        }

        // CRC over [hdr][len][bytes] — sync excluded.
        uint16_t computed = crc16_ccitt(&rxBuf[syncIdx + SYNC_SIZE], HEADER_SIZE + LEN_SIZE + plen);
        uint16_t received = (uint16_t)rxBuf[total - 2]
                          | ((uint16_t)rxBuf[total - 1] << 8);

        if (computed != received) {
            // False sync match or corrupted frame. Advance past this sync byte.
            scan_pos = syncIdx + 1;
            lastStatus = -3;
            continue;
        }

        // Valid frame. Extract.
        header = hdr;
        len = plen;
        if (plen > 0) memcpy(bytes, &rxBuf[syncIdx + SYNC_SIZE + HEADER_SIZE + LEN_SIZE], plen);

        int remaining = rxBufLen - total;
        if (remaining > 0) memmove(rxBuf, rxBuf + total, remaining);
        rxBufLen = remaining;
        scan_pos = 0;

        timeoutFlag = false;
        lastTimeoutClock = millis();
        return 1;
    }
}

int SerialManager::receiveMessage(uint8_t& header, float* data, uint8_t& numFloats)
{
    uint8_t buf[MAX_PAYLOAD];
    uint8_t len = 0;
    int rc = receiveMessage(header, buf, len);
    if (rc != 1) {
        numFloats = 0;
        return rc;
    }
    if (len % 4 != 0) {
        numFloats = 0;
        return -6;
    }
    numFloats = len / 4;
    if (numFloats > 0) memcpy(data, buf, len);
    return 1;
}
