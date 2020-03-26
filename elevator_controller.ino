#include <Adafruit_Soundboard.h>


#include <Wire.h> 
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

#define SFX_RST 3
#define SFX_ACT 4

Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, &Serial, SFX_RST);



#define DIR_UP 1
#define DIR_DOWN -1
#define FLOOR_PASSING_DELAY 1600
#define FLOOR_STOP_DELAY 3000

enum DoorState { OPEN, CLOSED, OPENING, CLOSING, NUDGE_MODE };
const char* doorStStr[] = {"Open","Closed","Opening", "Closing", "NudgeMode"};
DoorState doorState = CLOSED;

char floorPassingChimeFile[] = "PASSCHMEWAV";
char doorOpeningFile[] = "OPENDOORWAV";
char doorClosingFile[] = "CLSEDOORWAV";
char nudgeModeFile[] = "NUDGEMDEWAV";

//Pin configurations
#define FIRE_BUTTON_PIN 31
#define CALL_CANCEL_BUTTON_PIN 36
#define DOOR_OPEN_BUTTON_PIN 51
#define DOOR_CLOSE_BUTTON_PIN 43
#define EMERGENCY_PHONE_BUTTON_PIN 45
#define ALARM_BUTTON_PIN 53
#define NUM_BUTTONS 25
#define NUM_FLOOR_BUTTONS 19

//#define DEBUG

int buttonId2InputPin[NUM_BUTTONS] = {
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

int buttonId2OutputPin[NUM_BUTTONS] = {
  A11, //1  B2
  A6, //2  B1 //Need to look at pin
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


// = 30
//A5 = FIRE_BUTTON_OUT
//A13 = CALL_CANCEL_OUT


int floorButtonId2FloorNumber[NUM_FLOOR_BUTTONS] = {
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


//Shared
boolean isInFireButtonMode;
int currentFloor = 0;
boolean lastButtonState[NUM_BUTTONS];

//normal mode variables
boolean activeCalls[NUM_FLOOR_BUTTONS];
int currentDirection = 0;
unsigned long elevatorStartedMovingTimeMillis;
unsigned long doorOpenedTimeMillis;

boolean elevatorIsMoving = false;
boolean floorChanged;

//fire button mode variables
unsigned long  fireButtonModeStartedMillis;
boolean isButtonIlluminated[NUM_BUTTONS];
int ledDigitDisplayValue = 1;
boolean ledIsOn = true;

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

void printState() { 
  debugPrintLn("currentDirection=%d, elevatorIsMoving=%d, doorState=%s, currentFloor=%d", currentDirection, elevatorIsMoving, doorStStr[doorState], currentFloor);
}

boolean cancelButtonIsPressed() {
  int result = digitalRead(CALL_CANCEL_BUTTON_PIN);
  return result == 0;
}

void onCancelButtonPressed() {
  for(int i=0; i < NUM_FLOOR_BUTTONS; i++) {
    if(elevatorIsMoving && activeCalls[i] && currentDirection + currentFloor == i) {
      continue;
    }
    activeCalls[i] = false;
  }
 
  return;
}

/*
void fireButtonModeIteration() {
  int diff =  millis() - fireButtonModeStartedMillis;
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
    if(ledIsOn) {
      //Current floor rendering
      matrix.writeDigitNum(0, currentFloor / 10, false);
      matrix.writeDigitNum(1, currentFloor % 10, false);  
    }
    else {
      matrix.writeDigitRaw(0, 0);
      matrix.writeDigitRaw(0, 1);
    }
    
    //light up or turn off currently pressed buttons
    for(int i=0; i<NUM_BUTTONS; i++`) {
       illuminateButton(i, isButtonIlluminated[i]);
    }
}
*/


void startMoving() {
  debugPrintLn("Start moving");
  printState();
  elevatorIsMoving = true;
  elevatorStartedMovingTimeMillis = millis();
}

boolean elevatorHasReachedNextFloor() {
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

unsigned long doorOpenStartedTimeMillis;
unsigned long doorOpenFinishedTimeMillis;
unsigned long doorCloseStartedTimeMillis;
unsigned long nudgeModeStartedTimeMillis;

#define DOOR_OPEN_DELAY 3000
#define DOOR_CLOSING_DELAY 3000
#define DOOR_OPENING_DELAY 3000
#define MAX_DOOR_OPEN 10000
#define NUDGE_MODE_DURATION 6000

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
   if(currentDirection == 0) {
     return;
   }
   
  
   if(!elevatorIsMoving) {
     startMoving();
     return;
   }
   
   if(!elevatorHasReachedNextFloor()) {
      return;
   }
   
   changeFloors();
   elevatorIsMoving = false;
   if(activeCalls[currentFloor]) {
      unsetCall(currentFloor);
      startOpeningDoor();
   }
   else {
     startMoving();
   }
}

void loop() {

   /*
   enterOrExitFireButtonMode();
   if( isInFireButtonMode ) {
      fireButtonModeIteration();
      fireButtonModeRenderState();
      return;
   }
   */
   normalModeIteration();
   normalModeRenderState();

}

void changeFloors() {
   currentFloor = currentFloor + currentDirection;
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



/*
void enterOrExitFireButtonMode() {
  if(isInFireButtonMode) {
       if( isButtonPressed(CALL_CANCEL_BUTTON) ) {
          //leave fire button mode
          isInFireButtonMode = false;
          doorIsOpen = false;
          elevatorIsMoving = false;
          currentDirection = false;
       } 
       return;
  }

  if(isButtonPressed(FIRE_BUTTON) ) {
     isInFireButtonMode = true;
     fireButtonModeStartedMillis = millis();
     currentDirection = 0;
     elevatorIsMoving = false;
     doorIsOpen = true;
  }
  
}
*/

unsigned long lastMovementRenderTime = -1;
#define PROGRESS_MOVEMENT_DISPLAY_DELAY 100
boolean lastElevatorIsMovingState = false;
int segmentSequence[6] = { 1, 2, 4, 8, 16, 32 };
int sequenceIdx = 0;
boolean renderMovementIndicator() {
  boolean isDirty = false;
  //Clear screen if it stopped
  if(!elevatorIsMoving) {
     if(lastElevatorIsMovingState != elevatorIsMoving) {
        matrix.writeDigitRaw(3, 0);
        isDirty = true;
     }
     lastElevatorIsMovingState = elevatorIsMoving;
     return isDirty;
  }
  lastElevatorIsMovingState = elevatorIsMoving;
  
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

int lastFloor = -1;
int lastDirection = -1;
DoorState lastDoorState = CLOSED;
void normalModeRenderState() {
    int floorToDisplay = floorButtonId2FloorNumber[currentFloor];
    boolean dirty = false;
    if(lastFloor != currentFloor) {
      lastFloor = currentFloor;
      //Current floor rendering
      if(floorToDisplay == 82 || floorToDisplay == 81) {
         matrix.writeDigitRaw(0, 4 + 8 + 16 + 32 + 64);
      }
      else {
        matrix.writeDigitNum(0, floorToDisplay / 10, false); 
      }
      matrix.writeDigitNum(1, floorToDisplay % 10, false);  
      dirty = true;
    }

    dirty |= renderMovementIndicator();
    if(currentDirection != lastDirection) {
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
    

    if(floorChanged && elevatorIsMoving) {
      debugPrintLn("playing a sound");
      safeStop();
      if(!sfx.playTrack(floorPassingChimeFile)) {
        debugPrintLn("failed to play sound");
      }
    }

    if( lastDoorState != doorState) {
      if(doorState == OPENING) {
        safeStop();
        if(!sfx.playTrack(doorOpeningFile)) {
          debugPrintLn("failed to play sound");
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

void setup() {
  
  Serial.begin(9600);
  matrix.begin(0x70);
  Serial1.begin(9600); //Soundboard
  
  if (!sfx.reset()) {
    Serial.println("Not found");
    while (1);
  }
  
  Serial.println("SFX board found");

  pinMode(SFX_ACT, INPUT_PULLUP);
  
  for(int i=0; i < NUM_BUTTONS; i++) {
    pinMode(buttonId2OutputPin[i], OUTPUT);
    pinMode(buttonId2InputPin[i], INPUT_PULLUP);
  }
}
