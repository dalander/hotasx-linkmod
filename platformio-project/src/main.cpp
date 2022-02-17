/*
 * main.cpp
 *
 *  Created on: Februar 2022
 *      Author: Frank Weichert
 */

#include <Arduino.h>
#include <BleGamepad.h> 
#include <main.h>

//Remove // to activate debugging mode 
//#define DEBUG

BleGamepad *bleGamepad;
///  \def DEADZONE defines the range where joystick does not react
#define DEADZONE 100
///  \def MAXBUTTON defines the maximum number of used buttons (including switches)
#define MAXBUTTON 13

const int buttonPins[]  = {
                            /*BUTTONS*/   13,14,15,16,17,18,19,21
                            /*SWITCHES*/  ,26,27,32,33
                            /*JOYBUTTON*/ ,25
                            }; //!< defenition of the physical gpio buttonpins that are used 

int buttonPreviousValue[MAXBUTTON]; //!< internal use to detect edges

const int buttonMap[] =  {
                            /*BUTTONS*/   BUTTON_1,BUTTON_2,BUTTON_3,BUTTON_4,BUTTON_5,BUTTON_6,BUTTON_7,BUTTON_8
                            /*SWITCHES*/  ,BUTTON_9,BUTTON_10,BUTTON_11,BUTTON_12
                            }; //!< index of buttonpins matches the corrosponding BUTTON_ here 

const int xAxisPin = 34; //!< GPIO for x Axis
const int yAxisPin = 35; //!< GPIO for y Axis

// Used for autocalibration 
int xMid = 0;
int yMid = 0;
int xLow = 0;
int yLow = 0;
int xHigh=0;
int yHigh=0;

// variable for storing the mapped potentiometer value
int16_t xAxisValue = 0;
int16_t yAxisValue = 0;
int xAxisPreviousValue = 0;
int yAxisPreviousValue = 0;
int joyxAxisValue =  0;
int joyyAxisValue =  0;

void setup() {
  bleGamepad = new BleGamepad("HotasLinkmod","FWE",100);
  // Cleanup
  memset(buttonPreviousValue,0,MAXBUTTON*sizeof(int));

  #ifdef DEBUG 
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  #endif

  // Init Buttonpullups
  for (int buttonIndex=0;buttonIndex<MAXBUTTON;buttonIndex++){
      pinMode(buttonPins[buttonIndex],INPUT);
  }
  
  // Hope that system is started without tilted joystick to determine mid position
  xMid =  analogRead(xAxisPin);
  yMid =  analogRead(yAxisPin);

  // thanks lemmingdev for this library
  bleGamepad->begin(MAXBUTTON, 0,true, true,false, false, false, false, false,false, false, false, false, false, false);
}

void loop() {
  int joyxAxisValue =  analogRead(xAxisPin);
  int joyyAxisValue =  analogRead(yAxisPin);
  
  // Continuesly meassure boundaries
  if (joyxAxisValue < xLow)   xLow  = joyxAxisValue;
  if (joyxAxisValue > xHigh)  xHigh = joyxAxisValue;
  if (joyyAxisValue < yLow)   yLow  = joyyAxisValue;
  if (joyyAxisValue > yHigh)  yHigh = joyyAxisValue;

  // Strange looking mapping, but as ADC is 0-4095 and the mid position is not 2047 this should ensure
  // that the mid position is really centered. The remaining values 
  if (joyxAxisValue < xMid)
        xAxisValue = map ( joyxAxisValue,xLow,xMid,-32760,0);

  if (joyxAxisValue >= xMid)
        xAxisValue = map ( joyxAxisValue,xMid,xHigh,0,32760);

  if (joyyAxisValue < xMid)
        yAxisValue = map ( joyyAxisValue,yLow,yMid,-32760,0);

  if (joyyAxisValue >= xMid)
        yAxisValue = map ( joyyAxisValue,yMid,yHigh,0,32760);
        

  if(bleGamepad->isConnected()) {
    for (int buttonIndex=0;buttonIndex<MAXBUTTON;buttonIndex++){
      int currentEdge = detectEdge(buttonIndex);
     // Serial.printf("CurrentEdge = %3d\n",currentEdge);
      switch (currentEdge){
          case -1:
          bleGamepad->release(buttonMap[buttonIndex]);
          break;
        case 0:
          break;
        case 1:
          bleGamepad->press(buttonMap[buttonIndex]);
          break;
      }
    }

    #ifdef DEBUG
    Serial.printf("PIN %3d has value %3d->Mapped(%d)\n" ,xAxisPin,oxAxisValue,xAxisValue);
    Serial.printf("PIN %3d has value %3d->Mapped(%d)\n\n" ,yAxisPin,oyAxisValue, yAxisValue);
    #endif
  
    // Only send if something has been changed. Maybe it saves a bit transmission.
    if ((xAxisValue != xAxisPreviousValue) || yAxisValue != yAxisPreviousValue) { 
      bleGamepad->setAxes(xAxisValue,yAxisValue,0, 0,0, 0,0, 0, DPAD_CENTERED);
      xAxisPreviousValue=xAxisValue;
      yAxisPreviousValue=yAxisValue;
    }
    #ifdef DEBUG
    delay(500);
    #endif
  }
}

int16_t smoothDeadZone(int16_t value){
  if (abs(value) < DEADZONE)
  {
    return 0;
  }
  return value;
}

int detectEdge(int buttonIndex){
  int currentValue = digitalRead(buttonPins[buttonIndex]);
  #ifdef DEBUG
  Serial.printf("PIN=%3d: Index=%3d  with prev=%3d , now=%3d   -> ",buttonPins[buttonIndex] ,buttonIndex ,buttonPreviousValue[buttonIndex],currentValue);
  #endif
  if (currentValue > 0){
    // Detect if raising
    if ( buttonPreviousValue[buttonIndex] > 0){
      return 0; // no edge
    }else{
      buttonPreviousValue[buttonIndex] = currentValue;
      return 1; // positive edge
    }
  }else{
    //detect if falling
        if ( buttonPreviousValue[buttonIndex] <= 0){
      return 0; // no edge
    }else{
      buttonPreviousValue[buttonIndex] = currentValue;
      return -1; // negative edge
    }
  }
}