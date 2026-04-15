#include "Arduino_LED_Matrix.h"   // Include the LED_Matrix library

ArduinoLEDMatrix matrix;          // Create an instance of the ArduinoLEDMatrix class
uint8_t frame[8][12] = {0};

#include "SerialManager.h"

//SerialManager omniSoc(Serial,20);
SerialManager omniSoc(Serial,20);

long period_serial_ms = 100;
long serialClock = 0;

long period_debug_print_ms = 500;
long debugClock = 0;

float data[5] = {0,0,0,0,0}; //needs to be as biggest message to be received


void setup() {
  //Serial.begin(115200); //for debugging arduino to arduino coms with a mega
  omniSoc.connect(57600);
  matrix.begin(); 
  frame[0][0] = 1;
}

void toggleLED(int x,int y)
{
  if(frame[x][y] == 1)
  {
    frame[x][y] = 0;
  }
  else
  {
    frame[x][y] = 1;
  }
}

void loop() {
  matrix.renderBitmap(frame, 8, 12);

  omniSoc.handleSynchronization();

  //handle incoming message (assuming non-fixed frequency coms)
  {
    int header = 0;
    int numFloats = 0;
    //read arbitrary data
    int receiveCode = omniSoc.receiveMessage(header,data,numFloats);
    if(receiveCode == 1) //new message
    {
      toggleLED(0,0);

      //omnisoc testing
      //modify and reflect message
      {
        header+=1;
        omniSoc.sendMessage(header,data,numFloats);
      }
    }
  }

  //regular frequency messaging
  if(millis() - serialClock > period_serial_ms)
  {
    serialClock = millis();

    toggleLED(1,1);
    {
      int header = 1;
      float tempData[3] = {1.23, 4.56, (float)millis()};
      int numFloats = 3;
      //send arbitrary data
      int result = omniSoc.sendMessage(header,tempData,numFloats);

      if(result == -1)
      {
        toggleLED(5,5);
      }
      //Serial.println(result);
    }
  }

  //random messaging (for commands or non-regular coms)
  if(millis() - debugClock > period_debug_print_ms + random(1,100))
  {
    debugClock = millis();

    toggleLED(2,2);

    int header = 5;
    float tempData[1] = {5000};
    int numFloats = 1;
    //send arbitrary data
    int result = omniSoc.sendMessage(header,tempData,numFloats);
  }
}
