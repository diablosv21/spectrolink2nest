/*
  Spectrolink 2 Nest

 The circuit:
 * TX is digital pin 11

 This has been adapted to use Send Only Serial as there is no pin to listen to.

 'enableSerial' will put the code in to a test mode for debugging.
  This can easily be changed to still output to the Spectrolink controller at the same time.

 */
// #include <SoftwareSerial.h>
#include <SendOnlySoftwareSerial.h>
//#include <ReceiveOnlySoftwareSerial.h>

// SoftwareSerial mySerial(10, 11); // RX, TX
SendOnlySoftwareSerial txSerial (11);  // Tx pin

const int txBaudRate = 300;
const int rxBaudRate = 300; // 110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 128000 and 256000
long lastValue = 0;

const int messagePeriod = 1000; // Should be 1000 for one tick per second.
unsigned long lastMessage = 0;

byte sequenceIndex = 0;
byte messageType = 0;

//const byte messageSequenceCount = 16;
//const byte messageSequence[] = {1, 1, 1, 2, 1, 3, 1, 4, 1, 1, 1, 10, 1, 11, 1, 12}; // This is the usual send order
const byte messageSequenceCount = 1;
const byte messageSequence[] = {1}; // Disable 218 messages. They are only needed for slave units

byte output[8];

bool isCool = true;
bool isOn = false;

bool isZone1Open = true;
bool isZone2Open = true;
bool isZone3Open = true;

bool coolTestPin = HIGH;
bool heatTestPin = HIGH;
bool zone1TestPin = LOW;
bool zone2TestPin = HIGH;
bool zone3TestPin = HIGH;

const byte stateOff = 74;
const byte stateCooling = 202;
const byte stateHeating = 138;
const byte stateHeatingToCoolingOff = 90;
const byte stateCoolingToHeatingOff = 26;
const byte stateHeatingToOff = 10;

byte systemState = stateOff;

const byte zone123 = 167;
const byte zone23 = 166;
const byte zone13 = 165;
const byte zone3 = 164;
const byte zone12 = 163;
const byte zone2 = 162;
const byte zone1 = 161;
const byte zoneOff = 160;
const byte zoneOffHeat = 128;

byte zoneByte = zone123;

const int resetSendCount = 10;
int resetSendCounter = 10;

const int coolPin = 7;
const int heatPin = 8;

const int zone1Pin = 3;
const int zone2Pin = 4;
const int zone3Pin = 5;

const bool enableSerial = true;

const int heatingCooldownTime = 180; // 3 Minutes
int heatingCooldown = 0;

const int airconCooldownTime = 30; // 30 Seconds
int airconCooldown = 0;

const int heatingMaximumRuntime = 1800; // 30 Minutes
int heatingRuntime = 0; // 15 Minutes

const int autoResetTime = 300; // Reset the heater every 5 minutes while off
int autoReset = 0;

const int hysteresisWait = 5;
int hysteresis = 0;

void setup() {
  if(enableSerial) {
  // Open serial communications and wait for port to open:
    Serial.begin(9600);
    //while (!Serial) {
    //  ; // wait for serial port to connect. Needed for native USB port only
    //}

    Serial.println("Spectrolink Emulator");
  }
  
  txSerial.begin(txBaudRate);

  // Set these two pins to pullup for the heat/cool switching
  pinMode(coolPin, INPUT_PULLUP);
  pinMode(heatPin, INPUT_PULLUP);
  
  pinMode(zone1Pin, INPUT_PULLUP);
  pinMode(zone2Pin, INPUT_PULLUP);
  pinMode(zone3Pin, INPUT_PULLUP);

  // Set the starting Zone Byte as all zones are probably off.
  UpdateZoneByte();
  Reset();
}

void loop() {
  //ReadSpectrolinkInput();
  if(enableSerial) {
    ReadSerialInput(); 
  }

  if(millis() - lastMessage > messagePeriod) {
    // Tick the cooldown timers down once per second.
    CheckPins();
    
    cooldownTimersTick();
    
    SendMessage();
    lastMessage = millis();
  }
}

void cooldownTimersTick() {
  if(isOn) {
    // Track the maximum runtime for the heater
    if(!isCool) {
      heatingRuntime++;
    }
    return;
  }

  // Air con cooldown
  if(airconCooldown > 0) {
    airconCooldown--;
  }
  
  // Heating cooldown
  if(heatingCooldown > 0) {
    heatingCooldown--;
  }
    
  if(!isCool) {
    // Process Auto Reset
    if(autoReset > 0) {
      autoReset--;
    } else {
      Reset();
    }
  }
}

int readValue = 0;
int inputMessageCounter = 0;

void ReadSerialInput() {
  if(Serial.available()) {
    char command = Serial.read();
    switch (command) {
      case '1':
        zone1TestPin = !zone1TestPin;
        break;
      case '2':
        zone2TestPin = !zone2TestPin;
        break;
      case '3':
        zone3TestPin = !zone3TestPin;
        break;
      case 'h':
        heatTestPin = !heatTestPin;
        break;
      case 'c':
        coolTestPin = !coolTestPin;
        break;
    }
  }
}

void CheckPins() {
  int coolValue = digitalRead(coolPin);
  int heatValue = digitalRead(heatPin);
  
  if(enableSerial) {
    coolValue = coolTestPin;
    heatValue = heatTestPin;
  }
  
  if (coolValue == LOW && airconCooldown <= 0){
    if(hysteresis < hysteresisWait && !isOn) {
      hysteresis++;
    } else if(!isOn || !isCool) {
      isCool = true;
      isOn = true;
      systemState = stateCooling;

      if(enableSerial) {
        PrintOnState();
      }
    }
  } else if (heatValue == LOW && heatingCooldown <= 0 && heatingRuntime < heatingMaximumRuntime){
    if(hysteresis < hysteresisWait && !isOn) {
      hysteresis++;
    } else if(!isOn || isCool) {
      Reset(); // Perform a reset before starting the heat up. This delays start but hopefully means it works.
      
      isCool = false;
      isOn = true;
      systemState = stateHeating;
    }
  } else {
    if(hysteresis < hysteresisWait && isOn && heatingRuntime < heatingMaximumRuntime) {
      hysteresis++;
    } else if(isOn) {
      if(isCool) {
        // Heater is currently on and it now being asked to turn off. Start the cooldown timer!
        airconCooldown = airconCooldownTime;
      } else {
        // Heater is currently on and it now being asked to turn off. Start the cooldown timer!
        heatingCooldown = heatingCooldownTime;
      }

      if(heatingRuntime >= heatingMaximumRuntime && enableSerial) {
        Serial.println("Heater running too long. Turning off for periodic cool down.");
      }
      
      heatingRuntime = 0;
      isOn = false;
      systemState = stateOff;
      
      PrintOnState();
    }
  }
  
  isZone1Open = digitalRead(zone1Pin) == LOW;
  isZone2Open = digitalRead(zone2Pin) == LOW;
  isZone3Open = digitalRead(zone3Pin) == LOW;
  
  if(enableSerial) {
    isZone1Open = zone1TestPin;
    isZone2Open = zone2TestPin;
    isZone3Open = zone3TestPin;
  }

  if(isZone1Open && isZone2Open && isZone3Open && !isCool) {
    // The system is currently failing with all 3 zones on. Turn off Zone 1 in this situation so the system doesn't fail.
    isZone1Open = false;
  }
  
  UpdateZoneByte();
}

void SetCoolMode() {
  if(!isCool) {
    Reset();
    isCool = true;
  }
}

void SetHeatMode() {
  if(isCool) {
    Reset();
    isCool = false;
  }
}

void Reset() {
  resetSendCounter = resetSendCount;
  autoReset = autoResetTime;

  if(enableSerial) {
    Serial.println("Resetting...");
  }
}

void PrintZoneInfo(int zone, bool state) {
  Serial.print("Zone ");
  Serial.print(zone);
  Serial.print(" is now: ");
  if(state) {
    Serial.println("OPEN");
  } else {
    Serial.println("CLOSED");
  }
}

void PrintOnState() {
  Serial.print("System is: ");
  if(isOn) {
    Serial.print("ON");
  } else {
    Serial.print("OFF");
  }
  Serial.print(" - Mode: ");
  if(isCool) {
    Serial.print("Cool");
  } else {
    Serial.print("Heat");
  }
  Serial.print(" - Zones: ");
  if(isZone1Open) {
    Serial.print("1");
  } else {
    Serial.print("-");
  }
  if(isZone2Open) {
    Serial.print("2");
  } else {
    Serial.print("-");
  }
  if(isZone3Open) {
    Serial.print("3");
  } else {
    Serial.print("-");
  }
  Serial.println();
}

void UpdateZoneByte() {
  if(isZone1Open) {
    if(isZone2Open) {
      if(isZone3Open) {
        zoneByte = zone123;
      } else {
        zoneByte = zone12;
      }
    } else {
      if(isZone3Open) {
        zoneByte = zone13;
      } else {
        zoneByte = zone1;
      }
    }
  } else if (isZone2Open) {
    if(isZone3Open) {
      zoneByte = zone23;
    } else {
      zoneByte = zone2;
    }
  } else if (isZone3Open) {
    zoneByte = zone3;
  } else {
    zoneByte = zoneOff;
  }
}

void SendMessage() {
  sequenceIndex++;
  if(sequenceIndex >= messageSequenceCount) {
    sequenceIndex = 0;
  }

  messageType = messageSequence[sequenceIndex];
  if(messageType == 1) {
    CreateStandardMessage();
  } else if (messageType < 10) {
    CreateLowMessage();
  } else {
    CreateHighMessage();
  }
  WriteOutput();
  if(enableSerial && false) {
    WriteOutputToConsole();
  }
}

void CreateStandardMessage() {
  output[0] = 202;
  output[1] = 0;

  if(resetSendCounter > 0) {
    output[2] = zone123;
    output[3] = 32;
    output[4] = 0;
    output[5] = 0;
    output[6] = 0;
    output[7] = 126;

    resetSendCounter--;

    if(enableSerial && resetSendCounter <= 0) {
      Serial.println("Reset Complete");
      PrintOnState();
    }
  } else if(isOn) {
    output[2] = zoneByte;
    output[3] = 8;
    
    output[5] = 0;
    output[6] = 1;
    
    if(isCool) {
      output[4] = 128;

      switch (zoneByte) {
        case zone123:
          output[7] = 58;
          break;
        case zone23:
          output[7] = 79;
          break;
        case zone13:
          output[7] = 21;
          break;
        case zone3:
          output[7] = 96;
          break;
        case zone12:
          output[7] = 100;
          break;
        case zone2:
          output[7] = 17;
          break;
        case zone1:
          output[7] = 75;
          break;
        case zoneOff:
          output[2] = zoneOff;
          output[7] = 62;
          break;
      }
    } else {
      output[4] = 64;

      switch (zoneByte) {
        case zone123:
          output[7] = 144;
          break;
        case zone23:
          output[7] = 7;
          break;
        case zone13:
          output[7] = 93;
          break;
        case zone3:
          output[7] = 40;
          break;
        case zone12:
          output[7] = 44;
          break;
        case zone2:
          output[7] = 89;
          break;
        case zone1:
          output[7] = 3;
          break;
        case zoneOff:
          output[2] = zoneOffHeat;
          output[7] = 24;
          break;
      }
    }
  } else {
    output[2] = zoneByte;
    output[3] = 0;
    output[4] = 0;
    output[5] = 0;
    output[6] = 0;
    
    switch (zoneByte) {
      case zone123:
        output[7] = 104;
        break;
      case zone23:
        output[7] = 29;
        break;
      case zone13:
        output[7] = 71;
        break;
      case zone3:
        output[7] = 50;
        break;
      case zone12:
        output[7] = 54;
        break;
      case zone2:
        output[7] = 67;
        break;
      case zone1:
        output[7] = 25;
        break;
      case zoneOff:
        output[7] = 108;
        break;
    }
  }
}

void CreateLowMessage() {
  output[0] = 218;
  output[1] = messageType;
  output[2] = 17; // Minute
  output[3] = 15; // Hour
  output[4] = 3; // Day of Week
  output[5] = systemState;
  output[6] = 1; // Unknown
  
  if(messageType == 2) {
    switch (systemState) {
      case stateOff:
        output[7] = 104;
        break;
      case stateCooling:
        output[7] = 104;
        break;
      case stateHeating:
        output[7] = 12;
        break;
      case stateHeatingToCoolingOff:
        output[7] = 104;
        break;
      case stateCoolingToHeatingOff:
        output[7] = 104;
        break;
      case stateHeatingToOff:
        output[7] = 104;
        break;
    }
  } else if(messageType == 3) {
    switch (systemState) {
      case stateOff:
        output[7] = 104;
        break;
      case stateCooling:
        output[7] = 104;
        break;
      case stateHeating:
        output[7] = 104;
        break;
      case stateHeatingToCoolingOff:
        output[7] = 104;
        break;
      case stateCoolingToHeatingOff:
        output[7] = 104;
        break;
      case stateHeatingToOff:
        output[7] = 104;
        break;
    }
  } else if(messageType == 4) {
    switch (systemState) {
      case stateOff:
        output[7] = 104;
        break;
      case stateCooling:
        output[7] = 104;
        break;
      case stateHeating:
        output[7] = 27;
        break;
      case stateHeatingToCoolingOff:
        output[7] = 104;
        break;
      case stateCoolingToHeatingOff:
        output[7] = 104;
        break;
      case stateHeatingToOff:
        output[7] = 104;
        break;
    }
  }
}

void CreateHighMessage() {
  output[0] = 218;
  output[1] = messageType;
  
  if(isCool) {
    output[2] = 48;
    output[3] = 56;
    output[4] = 44;
    output[5] = systemState;
    output[6] = 48;

    if(messageType == 10) {
      switch (systemState) {
        case stateOff:
          output[7] = 104;
          break;
        case stateCooling:
          output[7] = 104;
          break;
        case stateHeating:
          output[7] = 104;
          break;
        case stateHeatingToCoolingOff:
          output[7] = 104;
          break;
        case stateCoolingToHeatingOff:
          output[7] = 104;
          break;
        case stateHeatingToOff:
          output[7] = 104;
          break;
      }
    } else if(messageType == 11) {
      switch (systemState) {
        case stateOff:
          output[7] = 104;
          break;
        case stateCooling:
          output[7] = 104;
          break;
        case stateHeating:
          output[7] = 104;
          break;
        case stateHeatingToCoolingOff:
          output[7] = 104;
          break;
        case stateCoolingToHeatingOff:
          output[7] = 104;
          break;
        case stateHeatingToOff:
          output[7] = 104;
          break;
      }
    } else if(messageType == 12) {
      switch (systemState) {
        case stateOff:
          output[7] = 104;
          break;
        case stateCooling:
          output[7] = 104;
          break;
        case stateHeating:
          output[7] = 104;
          break;
        case stateHeatingToCoolingOff:
          output[7] = 104;
          break;
        case stateCoolingToHeatingOff:
          output[7] = 104;
          break;
        case stateHeatingToOff:
          output[7] = 104;
          break;
      }
    }
  } else {
    output[2] = 40;
    output[3] = 30;
    output[4] = 42;
    output[5] = systemState;
    output[6] = 30;

    if(messageType == 10) {
      switch (systemState) {
        case stateOff:
          output[7] = 112;
          break;
        case stateCooling:
          output[7] = 112;
          break;
        case stateHeating:
          output[7] = 112;
          break;
        case stateHeatingToCoolingOff:
          output[7] = 112;
          break;
        case stateCoolingToHeatingOff:
          output[7] = 112;
          break;
        case stateHeatingToOff:
          output[7] = 112;
          break;
      }
    } else if(messageType == 11) {
      switch (systemState) {
        case stateOff:
          output[7] = 20;
          break;
        case stateCooling:
          output[7] = 20;
          break;
        case stateHeating:
          output[7] = 20;
          break;
        case stateHeatingToCoolingOff:
          output[7] = 20;
          break;
        case stateCoolingToHeatingOff:
          output[7] = 20;
          break;
        case stateHeatingToOff:
          output[7] = 20;
          break;
      }
    } else if(messageType == 12) {
      switch (systemState) {
        case stateOff:
          output[7] = 103;
          break;
        case stateCooling:
          output[7] = 103;
          break;
        case stateHeating:
          output[7] = 103;
          break;
        case stateHeatingToCoolingOff:
          output[7] = 103;
          break;
        case stateCoolingToHeatingOff:
          output[7] = 103;
          break;
        case stateHeatingToOff:
          output[7] = 103;
          break;
      }
    }
  }
}

void WriteOutputToConsole() {
  // Print Test Output to Console
  Serial.print("O:");
  Serial.print(output[0]);
  Serial.print(",");
  Serial.print(output[1]);
  Serial.print(",");
  Serial.print(output[2]);
  Serial.print(",");
  Serial.print(output[3]);
  Serial.print(",");
  Serial.print(output[4]);
  Serial.print(",");
  Serial.print(output[5]);
  Serial.print(",");
  Serial.print(output[6]);
  Serial.print(",");
  Serial.print(output[7]);
  Serial.println();
}

void WriteOutput() {
  // Output this to serial as proper bytes
  txSerial.write(output[0]);
  txSerial.write(output[1]);
  txSerial.write(output[2]);
  txSerial.write(output[3]);
  txSerial.write(output[4]);
  txSerial.write(output[5]);
  txSerial.write(output[6]);
  txSerial.write(output[7]);
}
