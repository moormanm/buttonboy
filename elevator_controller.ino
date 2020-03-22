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
#define INITIAL_DELAY 1000
#define FLOOR_PASSING_DELAY 1600
#define FLOOR_STOP_DELAY 3000

char floorPassingChimeFile[] = "floorPassingChime";
char floorStoppingChimeFile[] = "floorStoppingChime";

//Pin configurations
#define FIRE_BUTTON 24
#define CALL_CANCEL_BUTTON 23
#define NUM_BUTTONS 25
#define NUM_FLOOR_BUTTONS 19

#define DEBUG

int buttonId2InputPin[NUM_BUTTONS] = {
  47,
  39,
  41,
  30,
  49,
  34,
  46,
  29,
  44,
  28,
  42,
  37,
  40,
  35,
  50,
  32,
  48,
  33,
  38,
  36,
  31,
  43,
  51,
  53,
  45
};

int buttonId2OutputPin[NUM_BUTTONS] = {
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2
};

int floorButtonId2FloorNumber[NUM_FLOOR_BUTTONS] = {
  82,
  81,
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
boolean doorIsOpen = false;
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
  debugPrintLn("currentDirection=%d, elevatorIsMoving=%d, doorIsOpen=%d, currentFloor=%d", currentDirection, elevatorIsMoving, doorIsOpen, currentFloor);
}

/*
void onCancelButtonPressed() {
  for(int i=0; i < NUM_FLOOR_BUTTONS; i++) {
    activeCalls[i] = false;
  }
  currentDirection = 0;
  return;
}
*/
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
void tryToCloseDoor() {
  
  if( millis() - doorOpenedTimeMillis < FLOOR_STOP_DELAY ) {
      return; //nothing to do - leave the door open
  }
  debugPrintLn("Closing door");
  printState();
  doorIsOpen = false;
  return;
}

void startMoving() {
  debugPrintLn("Start moving");
  printState();
  elevatorIsMoving = true;
  elevatorStartedMovingTimeMillis = millis();
}

boolean elevatorHasReachedNextFloor() {
  return millis() - elevatorStartedMovingTimeMillis > FLOOR_PASSING_DELAY;
  
}

void openDoor() {
  doorOpenedTimeMillis = millis();
  doorIsOpen = true;
  debugPrintLn("Opening door");
  printState();
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


void normalModeIteration() {
   floorChanged = false;
   /*
   if( isButtonPressed(CALL_CANCEL_BUTTON) ) {
      onCancelButtonPressed();
      return;
   }
   */

   consumeNewFloorCalls();
   
   determineCurrentDirection();
   
   if(doorIsOpen) {
     tryToCloseDoor();
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
      openDoor();
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
     for(int i = currentFloor; i > 0; i--) {
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
int lastFloor = -1;
int lastDirection = -1;
void normalModeRenderState() {
    
    int floorToDisplay = floorButtonId2FloorNumber[currentFloor];
    if(lastFloor != currentFloor) {
      lastFloor = currentFloor;
      //Current floor rendering
      matrix.writeDigitNum(0, floorToDisplay / 10, false);
      matrix.writeDigitNum(1, floorToDisplay % 10, false);  
      matrix.writeDisplay();
    }

    if(currentDirection != lastDirection) {
       lastDirection = currentDirection;
       if(currentDirection == DIR_UP) {
          //matrix.writeDigitRaw(4,  2 + 4 + 8 + 16 + 32);   
       }
       else {
          //matrix.writeDigitRaw(4,  2 + 4 + 8 + 16 + 64);
       }
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
    

    if(floorChanged) {
      debugPrintLn("playing a sound");
      safeStop();
      if(elevatorIsMoving) {
        if(!sfx.playTrack("T02     WAV")) {
          debugPrintLn("failed to play sound");
        }
      }
      else {
        if(!sfx.playTrack("T02     WAV")) {
          debugPrintLn("failed to play sound");
        }
      }
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
   digitalWrite(buttonId, isOn);
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
