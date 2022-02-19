/*
 * main.cpp
 *
 *  Created on: Februar 2022
 *      Author: Frank Weichert
 */

#include <Arduino.h>
#include <BleGamepad.h> 
#include <main.h>
#include <EEPROM.h>

//Remove // to activate debugging mode 
#define DEBUG

BleGamepad *bleGamepad;
///  \def DEADZONE defines the range where joystick does not react
#define DEADZONE 100
///  \def MAXBUTTON defines the maximum number of used buttons (including switches)
#define MAXBUTTON 13
/// 13 Bytes in EEPROM, 6 int16_t and one byte check initialized status
#define EEPROMSIZE 14

///  \def SWAP_X_AXIS 1 -> Axis will be swapped, 0 -> Axis will be left like it is
#define SWAP_X_AXIS 1
///  \def SWAP_Y_AXIS 1 -> Axis will be swapped, 0 -> Axis will be left like it is
#define SWAP_Y_AXIS 1


/// defenition of the physical gpio buttonpins that are used 
const int buttonPins[]  = {
                            /*BUTTONS*/   23,22,21,19,18,5,17,16
                            /*SWITCHES*/  ,4,2,15,33
                            /*JOYBUTTON*/ ,32
                            }; 

int buttonPreviousValue[MAXBUTTON]; //!< internal use to detect edges

/// index of buttonpins matches the corrosponding BUTTON_ here 
const int buttonMap[] =  {
                            /*BUTTONS*/   BUTTON_1,BUTTON_2,BUTTON_3,BUTTON_4,BUTTON_5,BUTTON_6,BUTTON_7,BUTTON_8
                            /*SWITCHES*/  ,BUTTON_9,BUTTON_10,BUTTON_11,BUTTON_12
                            }; 

const int xAxisPin = 34; //!< GPIO for x Axis
const int yAxisPin = 35; //!< GPIO for y Axis

int16_t eepromUpdateLoopCounter = 0;  //!< Do not update every loop to save eeprom lifetime

// Used for autocalibration 
struct JCalibrationData {
  byte isInitialized = 1; //!< Determines if EEPROM has been used before
  byte dirtyFlag = 0;   //!< EEPROM write should only update not changed values, but to be sure we will only write if we detect changes, too.
  int16_t xMid =  0;  //!< x Mid Position taken from Start or NVRam
  int16_t yMid =  0;  //!< y Mid Position taken from Start or NVRam
  int16_t xLow =  0;  //!< x lowest Position determined during moving or at Start from NVRam
  int16_t yLow =  0;  //!< y lowest Position determined during moving or at Start from NVRam
  int16_t xHigh=  0;  //!< x highest Position determined during moving or at Start from NVRam
  int16_t yHigh=  0;  //!< y highest Position determined during moving or at Start from NVRam
} ;

JCalibrationData joyWorkData;

// variable for storing the mapped potentiometer value
int16_t xAxisValue = 0; //!< mapped x Axis value to BLE HID Range (-32767 - +32768)
int16_t yAxisValue = 0; //!< mapped y Axis value to BLE HID Range (-32767 - +32768)

int16_t xAxisPreviousValue = 0; //!< mapped x Axis value to BLE HID Range (-32767 - +32768) of previous loop
int16_t yAxisPreviousValue = 0; //!< mapped y Axis value to BLE HID Range (-32767 - +32768) of previous loop

int16_t joyxAxisValue =  0; //!< real Joystick analog input (0-4095) for x Axis
int16_t joyyAxisValue =  0; //!< real Joystick analog input (0-4095) for y Axis

void setup() {

  #ifdef DEBUG 
  Serial.begin(115200);
  Serial.println("HotasLinkmod starting.");
  #endif

  bleGamepad = new BleGamepad("HotasLinkmod","FWE",100);

  // Cleanup
  memset(buttonPreviousValue,0,MAXBUTTON*sizeof(int));
  
  EEPROM.begin(EEPROMSIZE);

  // Init Buttonpullups
  for (int buttonIndex=0;buttonIndex<MAXBUTTON;buttonIndex++){
      pinMode(buttonPins[buttonIndex],INPUT);
  }
  
  joyWorkData.xMid = analogRead(xAxisPin);
  joyWorkData.yMid = analogRead(yAxisPin);

  if (joyWorkData.xMid == 0 || joyWorkData.yMid==0){
    clearNVRAM();
  }
  
  readNVRAM();

  // thanks lemmingdev for this library
  bleGamepad->begin(MAXBUTTON, 0,true, true,false, false, false, false, false,false, false, false, false, false, false);
}

void loop() {
  eepromUpdateLoopCounter++;
  joyxAxisValue =  analogRead(xAxisPin);
  joyyAxisValue =  analogRead(yAxisPin);
  
  // Continuesly meassure boundaries
  updateCalibration();

  // Strange looking mapping, but as ADC is 0-4095 and the mid position is not 2047 this should ensure
  // that the mid position is really centered. The remaining values 
  if (joyxAxisValue < joyWorkData.xMid)
        xAxisValue = map ( joyxAxisValue,joyWorkData.xLow,joyWorkData.xMid,-32760,0);

  if (joyxAxisValue >= joyWorkData.xMid)
        xAxisValue = map ( joyxAxisValue,joyWorkData.xMid,joyWorkData.xHigh,0,32760);

  if (joyyAxisValue < joyWorkData.xMid)
        yAxisValue = map ( joyyAxisValue,joyWorkData.yLow,joyWorkData.yMid,-32760,0);

  if (joyyAxisValue >= joyWorkData.xMid)
        yAxisValue = map ( joyyAxisValue,joyWorkData.yMid,joyWorkData.yHigh,0,32760);



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

    #ifdef DEBUG2
    Serial.printf("Current Workdata\nisInitialized\t=\t%d\ndirtyFlag\t=\t%d\nxHigh\t=\t%d\nxLow\t=\t%d\nxMid\t=\t%d\nyHigh\t=\t%d\nyLow\t=\t%d\nyMid\t=\t%d\n",
      joyWorkData.isInitialized,
      joyWorkData.dirtyFlag,      
      joyWorkData.xHigh,
      joyWorkData.xLow,
      joyWorkData.xMid,
      joyWorkData.yHigh,
      joyWorkData.yLow,
      joyWorkData.yMid);
      Serial.printf("PIN %3d has value %3d->Mapped(%d)\n" ,xAxisPin,joyxAxisValue,xAxisValue);
      Serial.printf("PIN %3d has value %3d->Mapped(%d)\n\n" ,yAxisPin,joyyAxisValue, yAxisValue);
    #endif

    if (SWAP_X_AXIS == 1){
      xAxisValue *=-1;
    }

    if (SWAP_Y_AXIS == 1){
      yAxisValue *=-1;
    }
    // Only send if something has been changed. Maybe it saves a bit transmission.
    if ((xAxisValue != xAxisPreviousValue) || yAxisValue != yAxisPreviousValue) { 
      bleGamepad->setAxes(xAxisValue,yAxisValue,0, 0,0, 0,0, 0, DPAD_CENTERED);
      xAxisPreviousValue=xAxisValue;
      yAxisPreviousValue=yAxisValue;
    }

    #ifdef DEBUG2
      delay(1000);
    #endif
  }
}

int16_t respectDeadZone(int16_t value){
  if (abs(value) < DEADZONE)
  {
    return 0;
  }
  return value;
}

int detectEdge(int buttonIndex){
  int currentValue = digitalRead(buttonPins[buttonIndex]);
  #ifdef DEBUG2
    Serial.printf("PIN=%3d: Index=%3d  with prev=%3d , now=%3d   \n",buttonPins[buttonIndex] ,buttonIndex ,buttonPreviousValue[buttonIndex],currentValue);
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

void readNVRAM(){
  byte isInitialized = EEPROM.read(0); 
  #ifdef DEBUG
    Serial.printf("EEProm Initializer is=%d (255=unitialized)\n",isInitialized);
  #endif

  if (isInitialized == 0xFF)
  {
    //Uninitialized, start here
    // Hope that system is started without tilted joystick to determine mid position
    EEPROM.put(0,joyWorkData);
    #ifdef DEBUG
      Serial.printf("Initialized EEPROM with values\nisInitialized\t=\t%d\ndirtyFlag\t=\t%d\nxHigh\t=\t%d\nxLow\t=\t%d\nxMid\t=\t%d\nyHigh\t=\t%d\nyLow\t=\t%d\nyMid\t=\t%d\n",
      joyWorkData.isInitialized,
      joyWorkData.dirtyFlag,
      joyWorkData.xHigh,
      joyWorkData.xLow,
      joyWorkData.xMid,
      joyWorkData.yHigh,
      joyWorkData.yLow,
      joyWorkData.yMid);
    #endif
  }else{
    EEPROM.get(0,joyWorkData);
    #ifdef DEBUG
      Serial.printf("Read EEPROM Data values\nisInitialized\t=\t%d\ndirtyFlag\t=\t%d\nxHigh\t=\t%d\nxLow\t=\t%d\nxMid\t=\t%d\nyHigh\t=\t%d\nyLow\t=\t%d\nyMid\t=\t%d\n",
        joyWorkData.isInitialized,
        joyWorkData.dirtyFlag,
        joyWorkData.xHigh,
        joyWorkData.xLow,
        joyWorkData.xMid,
        joyWorkData.yHigh,
        joyWorkData.yLow,
        joyWorkData.yMid);
    #endif
  }
}

void updateCalibration(){
  // Continuesly meassure boundaries
  if (joyxAxisValue < joyWorkData.xLow)   {
    joyWorkData.xLow  = joyxAxisValue;
    joyWorkData.dirtyFlag=true;
  }
  if (joyxAxisValue > joyWorkData.xHigh){
      joyWorkData.xHigh = joyxAxisValue;
      joyWorkData.dirtyFlag=true;
  }
  if (joyyAxisValue < joyWorkData.yLow){
      joyWorkData.yLow  = joyyAxisValue;
      joyWorkData.dirtyFlag=true;
  }
  if (joyyAxisValue > joyWorkData.yHigh){
      joyWorkData.yHigh = joyyAxisValue;
      joyWorkData.dirtyFlag=true;
  }

  if (eepromUpdateLoopCounter%10000==0 && joyWorkData.dirtyFlag){
    joyWorkData.dirtyFlag=false;
    EEPROM.put(0,joyWorkData);
    #ifdef DEBUG
      Serial.printf("Save EEPROM @loop = %d with new values\nisInitialized\t=\t%d\ndirtyFlag\t=\t%d\nxHigh\t=\t%d\nxLow\t=\t%d\nxMid\t=\t%d\nyHigh\t=\t%d\nyLow\t=\t%d\nyMid\t=\t%d\n",
        eepromUpdateLoopCounter,
        joyWorkData.isInitialized,
        joyWorkData.dirtyFlag,
        joyWorkData.xHigh,
        joyWorkData.xLow,
        joyWorkData.xMid,
        joyWorkData.yHigh,
        joyWorkData.yLow,
        joyWorkData.yMid);
    #endif
    EEPROM.commit();
  }
}

void clearNVRAM(){
  #ifdef DEBUG
    Serial.printf("Cleanup NVRAM, restart to initialize again.\n");
  #endif 
  joyWorkData.isInitialized = 0xFF;
  joyWorkData.dirtyFlag     = 0;
  joyWorkData.xHigh         = 0;
  joyWorkData.xLow          = 0;
  joyWorkData.xMid          = 0;
  joyWorkData.yHigh         = 0;
  joyWorkData.yLow          = 0;
  joyWorkData.yMid          = 0;

  EEPROM.put(0,joyWorkData);
  EEPROM.commit();

  while(1){
    #ifdef DEBUG
      Serial.printf("Cleaned up NVRAM, waiting for restart.\n");
    #endif
    delay(5000);
  }
}