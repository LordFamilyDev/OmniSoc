#include "SerialManager.h"

//SerialManager omniSoc(Serial,20);
SerialManager omniSoc(Serial1,20);

long period_serial_ms = 10;
long serialClock = 0;

long period_debug_print_ms = 100;
long debugClock = 0;

float data[5] = {0,0,0,0,0}; //needs to be as biggest message to be received

void setup() {
  Serial.begin(115200);//115200

  omniSoc.connect(57600);
  pinMode(13,OUTPUT);
}

void loop() {
  omniSoc.handleSynchronization();

  //handle incoming message (assuming non-fixed frequency coms)
  {
    int header = 0;
    int numFloats = 0;
    //read arbitrary data
    int receiveCode = omniSoc.receiveMessage(header,data,numFloats);
    if(receiveCode == 1) //new message
    {
        Serial.print(header);
        Serial.print(",");
        Serial.print(numFloats);
        Serial.print(",");
        for(int i = 0;i<numFloats;i++)
        {
          Serial.print(data[i]);
          Serial.print(",");
        }
        Serial.println();
    }
  }

  //regular frequency messaging
  if(millis() - serialClock > period_serial_ms)
  {
    serialClock = millis();

    {
      int header = 1;
      float data[3] = {1.23, 4.56, (float)millis()};
      int numFloats = 3;
      //send arbitrary data
      int result = omniSoc.sendMessage(header,data,numFloats);
      //Serial.println(result);
    }
  }

  //random messaging (for commands or non-regular coms)
  if(millis() - debugClock > period_debug_print_ms + random(1,100))
  {
    debugClock = millis();

    int header = 5;
    float data[1] = {5000};
    int numFloats = 1;
    //send arbitrary data
    int result = omniSoc.sendMessage(header,data,numFloats);
  }

  //monitor connection and display connection status on pin 13
  if(omniSoc.isConnected())
  {
    digitalWrite(13,LOW);
  }
  else
  {
    digitalWrite(13,HIGH);
  }
}
