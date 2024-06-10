#include "SerialManager.h"

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

    int bytesAvail = serial->availableForWrite();
    if(bytesAvail < messageSize) 
    { return -1; }
    serial->write(message, messageSize);

    return 1;
}

int SerialManager::receiveMessage(int& header, float* data, int& numFloats) 
{
    if(!timeoutFlag && millis()-lastTimeoutClock > timeoutPeriod_ms)
    { timeoutFlag = true; }

    if(seekingFlag){return -5;}

    if(lastHeader == -1)
    {
        if (serial->available() < HEADER_SIZE + 1) 
        { return -1; }

        uint8_t headerMessage[HEADER_SIZE + 1];
        serial->readBytes(headerMessage, HEADER_SIZE + 1);

        header = (headerMessage[0] << 8) | headerMessage[1];
        numFloats = headerMessage[2];
        lastHeader = header;
        lastNumFloats = numFloats;

        if(numFloats>maxFloats)
        {
            seekingFlag = true;
            lastHeader = -1;
            lastNumFloats = 0;
            return -4;
        }
    }
    else
    {
        header = lastHeader;
        numFloats = lastNumFloats;
    }

    int floatDataSize = numFloats * FLOAT_SIZE;
    int dataAvailable = serial->available();
    if (dataAvailable < floatDataSize + CHECKSUM_SIZE) 
    { return -2; } //wait for complete message

    uint8_t messageSize = HEADER_SIZE + 1 + numFloats * FLOAT_SIZE + CHECKSUM_SIZE;
    uint8_t message[messageSize];

    //Add header to front of message for checksum
    message[0] = (header >> 8) & 0xFF;
    message[1] = header & 0xFF;
    message[2] = numFloats;

    uint8_t floatData[floatDataSize];
    serial->readBytes(floatData, floatDataSize);

    for(int i = 0;i<floatDataSize;i++)
    { message[i + HEADER_SIZE + 1]=floatData[i]; }

    lastHeader = -1;
    lastNumFloats = 0;

    for (int i = 0; i < numFloats; i++) {
        uint8_t floatBytes[FLOAT_SIZE];
        for (int j = 0; j < FLOAT_SIZE; j++) {
            floatBytes[j] = floatData[i * FLOAT_SIZE + j];
        }
        data[i] = *reinterpret_cast<float*>(floatBytes);
    }

    uint8_t receivedChecksum = serial->read();
    uint8_t computedChecksum = computeChecksum(message, messageSize - CHECKSUM_SIZE);

    if (receivedChecksum != computedChecksum) {
        //read everything on available to clear it until there is a break
        while(serial->available()>0)
        {serial->read();}

        seekingFlag = true;
        return -3;
    }

    timeoutFlag = false;
    lastTimeoutClock = millis();//reset timeout clock on successful message
    return 1;
}