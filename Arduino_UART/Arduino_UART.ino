//Arduino Mega test

#include "SerialManager.h"
#include "PackBytes.h"

SerialManager omniSoc(Serial2, 20);

const long period_telem_ms = 100;
long telemClock = 0;

const long period_debug_ms = 500;
long debugClock = 0;

// Reuse the same RX buffer every loop. Sized to the protocol max payload.
uint8_t rxBuf[SerialManager::MAX_PAYLOAD];


void setup() {
  omniSoc.connect(57600);
  Serial.begin(5700); //debug serial
}


void loop() {

  // MUST be called every loop iteration. receiveMessage() is the only thing
  // draining the HardwareSerial RX buffer; skipping calls > ~10ms (at 57600
  // baud) overflows the 64 B HW buffer and drops bytes silently.
  uint8_t header = 0;
  uint8_t len = 0;
  int receiveCode = omniSoc.receiveMessage(header, rxBuf, len);
  if (receiveCode == 1)  // new frame
  {

    // Echo back with header bumped — interop sanity check for testing.
    omniSoc.sendMessage(header + 1, rxBuf, len);
  }

  // Periodic telemetry: uint32 tick + float sample + uint32 millis() = 12 B.
  // Demonstrates mixed-type packing (the whole point of the bytes payload).
  if (millis() - telemClock > period_telem_ms)
  {
    telemClock = millis();

    uint8_t txBuf[12];
    uint8_t* p = txBuf;
    p = pack_u32(p, (uint32_t)telemClock);
    p = pack_float(p, 1.23f);
    p = pack_u32(p, (uint32_t)millis());

    int result = omniSoc.sendMessage(1, txBuf, (uint8_t)(p - txBuf));
  }

  // Sporadic debug message: single uint16 status code.
  if (millis() - debugClock > period_debug_ms + random(1, 100))
  {
    debugClock = millis();

    uint8_t dbgBuf[2];
    pack_u16(dbgBuf, 5000);
    omniSoc.sendMessage(5, dbgBuf, sizeof(dbgBuf));
  }
}
