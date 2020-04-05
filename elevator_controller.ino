//#define DEBUG

#include <Adafruit_Soundboard.h>
#include <Wire.h> 
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

//Soundboard related pins
#define SFX_RST 3
#define SFX_ACT 4

//Pin configurations
#define FIRE_BUTTON_PIN 31
#define CALL_CANCEL_BUTTON_PIN 36
#define DOOR_OPEN_BUTTON_PIN 51
#define DOOR_CLOSE_BUTTON_PIN 43
#define EMERGENCY_PHONE_BUTTON_PIN 45
#define ALARM_BUTTON_PIN 53
#define NUM_BUTTONS 25
#define NUM_FLOOR_BUTTONS 19

//Various constants
#define DIR_UP 1
#define DIR_DOWN -1
#define FLOOR_PASSING_DELAY 1600
#define EXPRESS_ZONE_FLOOR_PASSING_DELAY 5000 
#define FLOOR_STOP_DELAY 3000
#define DOOR_OPEN_DELAY 3000
#define DOOR_CLOSING_DELAY 3000
#define DOOR_OPENING_DELAY 3000
#define MAX_DOOR_OPEN 10000
#define NUDGE_MODE_DURATION 6000
#define PROGRESS_MOVEMENT_DISPLAY_DELAY 100

const int buttonId2InputPin[NUM_BUTTONS] = {
  47, //0
  39, //1
  41, //2
  30, //3
  49, //4
  34, //5
  46, //6
  29, //7
  44, //8
  28, //9
  42, //10
  37, //11
  40, //12
  35, //13
  50, //14
  32, //15
  48, //16
  33, //17
  38, //18
  CALL_CANCEL_BUTTON_PIN, //19
  FIRE_BUTTON_PIN, //20
  DOOR_CLOSE_BUTTON_PIN, //21
  DOOR_OPEN_BUTTON_PIN, //22
  ALARM_BUTTON_PIN, //23
  EMERGENCY_PHONE_BUTTON_PIN //24
};

const int buttonId2OutputPin[NUM_BUTTONS] = {
  A11, //1  B2
  A6, //2  B1 
  2, //3  1
  A14, //4  20 
  A0, //5  21
  25, //6  22
  A4, //7 23
  24, //8 24
  A9,  //9 25
  17, //10 26 
  A13, //11 27
  A2, //12 28
  A8, //13  29
  22, //14   30
  A12, //15 31  
  23,  //16 32
  A1, //17 33
  26, //18  34
  A5, //19  35
  13, //20  //CANCEL
  27, //21  //FIRE
  12, //22 //CLOSE  
  A15, //23  //OPEN
  A7, //24  //ALARM
  A3 //25   //EMERGENCY
 };

const int floorButtonId2FloorNumber[NUM_FLOOR_BUTTONS] = {
  82, //82 is a poor man's B2
  81, //81 is a poor man's B1
  1,
  20,
  21,
  22,
  23,
  24,
  25,
  26,
  27,
  28,
  29,
  30,
  31,
  32,
  33,
  34,
  35  
};

//Sound files
char doorClosingFile[] = "CLSEDOORWAV";
char nudgeModeFile[] = "NUDGEMDEWAV";
char helpFireFile[] = "HELPFIREWAV";
char normalChimeFile[] = "PASSCHMEWAV";
char dieselDucyFile[] = "DIESELDUWAV";
const char* doorOpenSoundWithNoDirection[NUM_FLOOR_BUTTONS] = {
  "OD-B2   OGG",
  "OD-B1   OGG",
  "OD-L    OGG",
  "OD-20   OGG",
  "OD-21   OGG",
  "OD-22   OGG",
  "OD-23   OGG",
  "OD-24   OGG",
  "OD-25   OGG",
  "OD-26   OGG",
  "OD-27   OGG",
  "OD-28   OGG",
  "OD-29   OGG",
  "OD-30   OGG",
  "OD-31   OGG",
  "OD-32   OGG",
  "OD-33   OGG",
  "OD-34   OGG",
  "OD-35   OGG"
};
const char* doorOpenSoundWithUpDirection[NUM_FLOOR_BUTTONS] = {
  "OD-B2-GUOGG",
  "OD-B1-GUOGG",
  "OD-L-GU OGG",
  "OD-20-GUOGG",
  "OD-21-GUOGG",
  "OD-22-GUOGG",
  "OD-23-GUOGG",
  "OD-24-GUOGG",
  "OD-25-GUOGG",
  "OD-26-GUOGG",
  "OD-27-GUOGG",
  "OD-28-GUOGG",
  "OD-29-GUOGG",
  "OD-30-GUOGG",
  "OD-31-GUOGG",
  "OD-32-GUOGG",
  "OD-33-GUOGG",
  "OD-34-GUOGG",
  "OD-35-GUOGG"
};

const char* doorOpenSoundWithDownDirection[NUM_FLOOR_BUTTONS] = {
  "OD-B2-GDOGG",
  "OD-B1-GDOGG",
  "OD-L-GD OGG",
  "OD-20-GDOGG",
  "OD-21-GDOGG",
  "OD-22-GDOGG",
  "OD-23-GDOGG",
  "OD-24-GDOGG",
  "OD-25-GDOGG",
  "OD-26-GDOGG",
  "OD-27-GDOGG",
  "OD-28-GDOGG",
  "OD-29-GDOGG",
  "OD-30-GDOGG",
  "OD-31-GDOGG",
  "OD-32-GDOGG",
  "OD-33-GDOGG",
  "OD-34-GDOGG",
  "OD-35-GDOGG"
};

enum DoorState { OPEN, CLOSED, OPENING, CLOSING, NUDGE_MODE };
const char* doorStStr[] = {"Open","Closed","Opening", "Closing", "NudgeMode"};
const int segmentSequence[6] = { 1, 2, 4, 8, 16, 32 };

//7 segment display and soundboard
Adafruit_7segment matrix = Adafruit_7segment();
Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, &Serial, SFX_RST);

//Global variables related to simulation state
DoorState doorState = CLOSED;
unsigned long doorOpenStartedTimeMillis;
unsigned long doorOpenFinishedTimeMillis;
unsigned long doorCloseStartedTimeMillis;
unsigned long nudgeModeStartedTimeMillis;
unsigned long elevatorStartedMovingTimeMillis;
unsigned long doorOpenedTimeMillis;
boolean isInFireButtonMode;
int currentFloor = 2;
boolean activeCalls[NUM_FLOOR_BUTTONS];
int currentDirection = 0;
boolean floorChanged;
boolean ledIsOn = true;
int currentMovementDirection = 0;


//Rendering related variables
char* floorPassingChimeFile = normalChimeFile;
DoorState lastDoorState = CLOSED;
boolean lastElevatorIsMovingState = false;
unsigned long lastMovementRenderTime = -1;
int lastFloor = -1;
int lastDirection = -1;
int sequenceIdx = 0;
unsigned long  fireButtonModeStartedMillis;
boolean lastButtonState[NUM_BUTTONS];
boolean forceDraw = false;
boolean lastLedIsOnState;
boolean isButtonIlluminated[NUM_BUTTONS];
boolean lastExpressZoneState;

void debugPrintLn(const char* fmt, ...) {
  #ifdef DEBUG
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf,fmt, args);
    Serial.println(buf);
    va_end(args);
  #endif
}


boolean elevatorIsMoving() {
  return currentMovementDirection != 0;
}

void printState() { 
  debugPrintLn("currentDirection=%d, elevatorIsMoving=%d, doorState=%s, currentFloor=%d", currentDirection, currentMovementDirection, doorStStr[doorState], currentFloor);
}

boolean cancelButtonIsPressed() {
  int result = digitalRead(CALL_CANCEL_BUTTON_PIN);
  return result == 0;
}
boolean emergencyPhoneButtonIsPressed() {
  int result = digitalRead(EMERGENCY_PHONE_BUTTON_PIN);
  return result == 0;  
}
boolean alarmButtonIsPressed() {
  int result = digitalRead(ALARM_BUTTON_PIN);
  return result == 0;  
}

void onCancelButtonPressed() {
  for(int i=0; i < NUM_FLOOR_BUTTONS; i++) {
    activeCalls[i] = false;
  }
  return;
}

void fireButtonModeIteration() {
  unsigned long diff =  millis() - fireButtonModeStartedMillis;
  int segment = diff % 2000;
  boolean illuminated = true;
  if(segment >= 1000) {
     illuminated = false;
  }
  for(int i=0; i<NUM_BUTTONS; i++) {
    isButtonIlluminated[i] = illuminated;
  }
  ledIsOn = illuminated;
}

void fireButtonModeRenderState() {
    if(lastLedIsOnState == ledIsOn) {
      return;
    }
    lastLedIsOnState = ledIsOn;
    if(ledIsOn) {
       matrix.writeDigitRaw(0, 2 + 4  + 16 + 32 + 64 ); // H
       matrix.writeDigitRaw(1, 1 + 8 + 16 + 32 + 64);   // E 
       matrix.writeDigitRaw(3, 8 + 16 + 32);            // L
       matrix.writeDigitRaw(4, 1 + 2 + 16 + 32 + 64 );  // P
       int isPlaying = digitalRead(SFX_ACT);
       if (isPlaying != LOW) {
          sfx.playTrack(helpFireFile);
       }
    }
    else {
      matrix.writeDigitRaw(0, 0);
      matrix.writeDigitRaw(1, 0);
      matrix.writeDigitRaw(2, 0);
      matrix.writeDigitRaw(3, 0);
      matrix.writeDigitRaw(4, 0);
    }
    matrix.writeDisplay();
    //light up or turn off currently pressed buttons
    for(int i=0; i<NUM_BUTTONS; i++) {
       illuminateButton(i, isButtonIlluminated[i]);
    }
}

void startMoving() {
  debugPrintLn("Start moving");
  printState();
  currentMovementDirection = currentDirection;
  elevatorStartedMovingTimeMillis = millis();
}

boolean isInExpressZone() {
  return currentFloor == 2 && currentMovementDirection == 1 || currentFloor == 3 && currentMovementDirection == -1;
}
boolean elevatorHasReachedNextFloor() {
  
  if(isInExpressZone()) {
    return millis() - elevatorStartedMovingTimeMillis > EXPRESS_ZONE_FLOOR_PASSING_DELAY;
  }
  return millis() - elevatorStartedMovingTimeMillis > FLOOR_PASSING_DELAY;  
}

boolean doorOpenButtonIsPressed() {
  int result = digitalRead(DOOR_OPEN_BUTTON_PIN);
  return result == 0;
}

void unsetCall(int buttonId) {
  debugPrintLn("Unsetting call %d\n", buttonId);
  printState();
  activeCalls[buttonId] = false;
}

void setCall(int buttonId) {
  debugPrintLn("Setting call %d\n", buttonId);
  printState();
  activeCalls[buttonId] = true;
}

void startOpeningDoor() {
  doorOpenStartedTimeMillis = millis();
  doorState = OPENING;
  debugPrintLn("Opening door");
  printState();
}

void updateDoorState() {
  if(doorState == OPEN && millis() - doorOpenFinishedTimeMillis > DOOR_OPEN_DELAY) {
    doorState = CLOSING;
    doorCloseStartedTimeMillis = millis();
  }
  else if( doorState == CLOSING && millis() - doorCloseStartedTimeMillis > DOOR_CLOSING_DELAY) {
    doorState = CLOSED; 
  }
  else if( doorState == OPENING && millis() - doorOpenStartedTimeMillis > DOOR_OPENING_DELAY) {
    doorState = OPEN;
    doorOpenFinishedTimeMillis = millis();
  }
  else if( doorState == OPEN && millis() - doorOpenStartedTimeMillis > MAX_DOOR_OPEN) {
    doorState = NUDGE_MODE;
    nudgeModeStartedTimeMillis = millis();
  }
  else if( doorState == NUDGE_MODE && millis() - nudgeModeStartedTimeMillis > NUDGE_MODE_DURATION) {
    doorState = CLOSED;
  }
  else {
    if(doorState == OPEN && doorOpenButtonIsPressed()) {
      //Extend the timer
      doorOpenFinishedTimeMillis = millis();
    }
    if(doorState == CLOSING && doorOpenButtonIsPressed()) {
      doorState = OPENING; 
    }
    return;
  }
  printState();
}

void normalModeIteration() {
   floorChanged = false;

   //implement rowie's optional chiming logic
   if( emergencyPhoneButtonIsPressed() ) {
       floorPassingChimeFile = normalChimeFile; 
   }
   else if( alarmButtonIsPressed() ) {
       floorPassingChimeFile = dieselDucyFile;
   }

   if( cancelButtonIsPressed( ) ) {
      onCancelButtonPressed();
      return;
   }
   
   consumeNewFloorCalls();
   
   determineCurrentDirection();

   updateDoorState();
   if(doorState != CLOSED) {
     return;
   }
   
   //No calls are set - nothing to do
   if(currentDirection == 0 && !elevatorIsMoving()) {
     return;
   }
   
   if(!elevatorIsMoving()) {
     startMoving();
     return;
   }
   
   if(!elevatorHasReachedNextFloor()) {
      return;
   }
   
   changeFloors();
   currentMovementDirection = 0;
   if(activeCalls[currentFloor]) {
      unsetCall(currentFloor);
      determineCurrentDirection();
      startOpeningDoor();
   }
   else {
     startMoving();
   }
}


void changeFloors() {
   currentFloor = currentFloor + currentMovementDirection;
   floorChanged = true;
   debugPrintLn("Changing floors to %d",currentFloor);
   printState();
}

void determineCurrentDirection() {
   if( currentDirection == 0 ) {
      int activeCallForFloor = -1;
      for(int i=0; i<NUM_FLOOR_BUTTONS; i++) {
        if(activeCalls[i]) {
            activeCallForFloor = i;
        }
      }
      if( activeCallForFloor == -1) {
        return;
      }
      if(currentFloor > activeCallForFloor) {
        currentDirection = DIR_DOWN;
        debugPrintLn("Going down");
        printState();
      }
      else if(currentFloor < activeCallForFloor) {
        currentDirection = DIR_UP;
        debugPrintLn("Going up");
        printState();
      }
      return;
   }
   
   else if(currentDirection == DIR_UP) {
     boolean activeCallsPending = false;
     //Turn off current direction if there are no more floors above this one
     for(int i = currentFloor; i < NUM_FLOOR_BUTTONS; i++) {
       if(activeCalls[i]) {
         activeCallsPending = true;
         break;
       }
     }
     if(!activeCallsPending) {
       debugPrintLn("No more calls pending while going up, setting it back to 0");
       currentDirection = 0;
       printState();
     }
   }
   else if(currentDirection == DIR_DOWN) {
     boolean activeCallsPending = false;
     //Turn off current direction if there are no more floors above this one
     for(int i = currentFloor; i >= 0; i--) {
       if(activeCalls[i]) {
         activeCallsPending = true;
         break;
       }
     }
     if(!activeCallsPending) {
       currentDirection = 0;
       debugPrintLn("No more calls pending while going down, setting it back to 0");
       printState();
     }
   }
}

void consumeNewFloorCalls() {
   char buf[255];
   for(int i=0; i<NUM_FLOOR_BUTTONS; i++) {
      boolean st = isButtonPressed(i);
      if(st == lastButtonState[i]) {
        continue;
      }
      
      debugPrintLn("Button state changed: %d - %d - %d \n", i, buttonId2InputPin[i], st);
      printState();
      lastButtonState[i] = st;
      if(!st) {
        continue;
      }
      
      if(i > currentFloor && currentDirection == DIR_UP) {
          setCall(i); 
      }
      else if(i < currentFloor && currentDirection == DIR_DOWN) {
          setCall(i); 
      }
      else if( currentDirection == 0 && currentFloor != i) {
          setCall(i);
      }      
   }
}

boolean fireButtonIsPressed() {
  int result = digitalRead(FIRE_BUTTON_PIN);
  return result == 0;
}

void enterOrExitFireButtonMode() {
  if(isInFireButtonMode) {
       if( cancelButtonIsPressed() ) {
          //leave fire button mode
          isInFireButtonMode = false;
          currentDirection = 0;
          currentMovementDirection = 0;
          doorState = CLOSED;
          forceDraw = true;
       } 
       return;
  }

  if(fireButtonIsPressed() ) {
     isInFireButtonMode = true;
     fireButtonModeStartedMillis = millis();
     currentDirection = 0;
     currentMovementDirection = 0;
     doorState = OPEN;
  }
  
}

boolean renderMovementIndicator() {
  boolean isDirty = false;
  //Clear screen if it stopped
  if(!elevatorIsMoving()) {
     if(lastElevatorIsMovingState != elevatorIsMoving()) {
        matrix.writeDigitRaw(3, 0);
        isDirty = true;
     }
     lastElevatorIsMovingState = elevatorIsMoving();
     return isDirty;
  }
  lastElevatorIsMovingState = elevatorIsMoving();
  
  if(millis() - lastMovementRenderTime < PROGRESS_MOVEMENT_DISPLAY_DELAY) {
    return false;
  }
  lastMovementRenderTime = millis();
  matrix.writeDigitRaw(3, segmentSequence[sequenceIdx]);
  sequenceIdx++;
  if(sequenceIdx == 6) {
    sequenceIdx = 0;
  }
  return true; 
}

void getDoorOpenFile(char* buf, int currentFloorIdx, int currentDirection) {
  const char* ret;  
  if(currentDirection == DIR_UP) {
    ret = doorOpenSoundWithUpDirection[currentFloorIdx];
  }
  else if(currentDirection == DIR_DOWN) {
    ret =  doorOpenSoundWithDownDirection[currentFloorIdx];
  }
  else {
    ret = doorOpenSoundWithNoDirection[currentFloorIdx]; 
  }
  sprintf(buf, ret);
}

void normalModeRenderState() {
    int floorToDisplay = floorButtonId2FloorNumber[currentFloor];
    boolean dirty = false;
    if(lastFloor != currentFloor || forceDraw || lastExpressZoneState != isInExpressZone() ) {
      lastFloor = currentFloor;
      lastExpressZoneState = isInExpressZone();
      //Current floor rendering
      if(floorToDisplay == 82 || floorToDisplay == 81) {
         matrix.writeDigitRaw(0, 4 + 8 + 16 + 32 + 64);
         matrix.writeDigitNum(1, floorToDisplay % 10, false);
      }
      else if( isInExpressZone() ) {
        matrix.writeDigitRaw(0, 1 + 8 + 16 + 32 + 64); // E
        matrix.writeDigitRaw(1, 1 + 2 + 16 + 32 + 64); // P
      
      }
      else if( floorToDisplay == 1 ) { //Floor L
        matrix.writeDigitRaw(0, 8 + 16 + 32);
        matrix.writeDigitRaw(1, 0);
        
      }
      else {
        matrix.writeDigitNum(0, floorToDisplay / 10, false); 
        matrix.writeDigitNum(1, floorToDisplay % 10, false);
      }
      
      matrix.writeDigitRaw(3, 0);  
      dirty = true;
    }

    dirty |= renderMovementIndicator();
    if(currentDirection != lastDirection || forceDraw) {
       dirty = true;
       lastDirection = currentDirection;
       if(currentDirection == DIR_UP) {
          matrix.writeDigitRaw(4,  2 + 4 + 8 + 16 + 32);   
       }
       else if(currentDirection == DIR_DOWN) { 
          matrix.writeDigitRaw(4,  2 + 4 + 8 + 16 + 64);
       }
       else {
          matrix.writeDigitRaw(4,  0); 
       }
    }
    if(dirty) {
      matrix.writeDisplay();
    }
    //light up or turn off currently pressed buttons
    for(int i=0; i<NUM_BUTTONS; i++) {
       boolean illuminated;
    
       if(isFloorButton(i)) {
          illuminated = activeCalls[i] || isButtonPressed(i);
       }
       else {
          illuminated = isButtonPressed(i);
       }
       illuminateButton(i, illuminated);
    }
    

    if(floorChanged && elevatorIsMoving()) {
      debugPrintLn("playing a sound");
      safeStop();
      if(!sfx.playTrack(floorPassingChimeFile)) {
        debugPrintLn("failed to play sound");
      }
    }

    if( lastDoorState != doorState) {
      if(doorState == OPENING) {
        safeStop();
        char buf[12];
        getDoorOpenFile(buf, currentFloor, currentDirection);
        if(!sfx.playTrack(buf)) {
          debugPrintLn("failed to play sound %s ", buf);
        }
      }
      else if( doorState == CLOSING) {
        safeStop();
        if(!sfx.playTrack(doorClosingFile)) {
          debugPrintLn("failed to play sound");
        }
      }
      else if( doorState == NUDGE_MODE) {
        safeStop();
        if(!sfx.playTrack(nudgeModeFile)) {
          debugPrintLn("failed to play sound");
        }
      }
      lastDoorState = doorState;
    }
    forceDraw = false;
    
}

boolean isFloorButton(int buttonId) {
  return buttonId >= 0 && buttonId < NUM_FLOOR_BUTTONS;
}

boolean isButtonPressed(int buttonId) {
  int result = digitalRead( buttonId2InputPin[buttonId]);
  return result == 0;
}

void illuminateButton(int buttonId, boolean isOn) {
   digitalWrite(buttonId2OutputPin[buttonId], isOn);
}

void safeStop() {
  int isPlaying = digitalRead(SFX_ACT);
  if (isPlaying == LOW) {
    sfx.stop();
  }
}

//Arduino entry point - this is called endlessly
void loop() {
   enterOrExitFireButtonMode();
   if( isInFireButtonMode ) {
      fireButtonModeIteration();
      fireButtonModeRenderState();
      return;
   }
   
   normalModeIteration();
   normalModeRenderState();
}

//Arduino entry point
void setup() {
  #ifdef DEBUG
  Serial.begin(9600);
  #endif 
  
  matrix.begin(0x70);
  Serial1.begin(9600); //Soundboard
  
  if (!sfx.reset()) {
    while (1); //Hang if the soundboard could not be reset successfully - the led will contain junk text in this case
  }
  
  //Configure the activity detection pin for the sfx card - this is a workaround to be able to send a 'stop' command if a sound is currently playing
  pinMode(SFX_ACT, INPUT_PULLUP);
  
  //Configure all of the button pins, one to handle the momentary push button signal, one output to control the led
  for(int i=0; i < NUM_BUTTONS; i++) {
    pinMode(buttonId2OutputPin[i], OUTPUT);
    pinMode(buttonId2InputPin[i], INPUT_PULLUP);
  }
}
