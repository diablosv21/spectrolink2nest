/*
  Software serial multple serial test

 Receives from the hardware serial, sends to software serial.
 Receives from software serial, sends to hardware serial.

 The circuit:
 * RX is digital pin 10 (connect to TX of other device)
 * TX is digital pin 11 (connect to RX of other device)

 Note:
 Not all pins on the Mega and Mega 2560 support change interrupts,
 so only the following can be used for RX:
 10, 11, 12, 13, 50, 51, 52, 53, 62, 63, 64, 65, 66, 67, 68, 69

 Not all pins on the Leonardo and Micro support change interrupts,
 so only the following can be used for RX:
 8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI).

 created back in the mists of time
 modified 25 May 2012
 by Tom Igoe
 based on Mikal Hart's example

 This example code is in the public domain.

 */
#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

const int baudRate = 300;
long lastValue = 0;

const int messagePeriod = 1000;
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

const int resetSendCount = 30;
int resetSendCounter = 10;

int coolPin = 7;
int heatPin = 8;

int zone1Pin = 3;
int zone2Pin = 4;
int zone3Pin = 5;

bool enableSerial = false;

void setup() {
  if(enableSerial) {
  // Open serial communications and wait for port to open:
    Serial.begin(9600);
    //while (!Serial) {
    //  ; // wait for serial port to connect. Needed for native USB port only
    //}

    Serial.println("Spectrolink Emulator");
  }
  
  mySerial.begin(baudRate);

  // Set these two pins to pullup for the heat/cool switching
  pinMode(coolPin, INPUT_PULLUP);
  pinMode(heatPin, INPUT_PULLUP);
  
  pinMode(zone1Pin, INPUT_PULLUP);
  pinMode(zone2Pin, INPUT_PULLUP);
  pinMode(zone3Pin, INPUT_PULLUP);

  // Set the starting Zone Byte as all zones are probably off.
  UpdateZoneByte();
}

void loop() {
  if(enableSerial) {
    ReadSpectrolinkInput();
  }
  //ReadSerialInput();
  CheckPins();

  if(millis() - lastMessage > messagePeriod) {
    SendMessage();
    lastMessage = millis();
  }
}

void ReadSpectrolinkInput() {
  if (mySerial.available()) {
    if(millis() - lastValue > 200) {
      Serial.println();
    }
    
    int value = mySerial.read();
    Serial.print(value);
    Serial.write(',');

    lastValue = millis();
  }
}

void ReadSerialInput() {
  if(Serial.available()) {
    char command = Serial.read();
    switch (command) {
      case 'o':
        isOn = !isOn;
        PrintOnState();
        break;
      case '1':
        isZone1Open = !isZone1Open;
        UpdateZoneByte();
        PrintZoneInfo(1, isZone1Open);
        break;
      case '2':
        isZone2Open = !isZone2Open;
        UpdateZoneByte();
        PrintZoneInfo(2, isZone2Open);
        break;
      case '3':
        isZone3Open = !isZone3Open;
        UpdateZoneByte();
        PrintZoneInfo(3, isZone3Open);
        break;
      case 'h':
        SetHeatMode();
        PrintOnState();
        break;
      case 'c':
        SetCoolMode();
        PrintOnState();
        break;
    }
  }
}

void CheckPins() {
  int coolValue = digitalRead(coolPin);
  int heatValue = digitalRead(heatPin);
  if (coolValue == LOW){
    isCool = true;
    isOn = true;
    systemState = stateCooling;
  } else if (heatValue == LOW){
    if(!isOn) {
      Reset(); // Perform a reset before starting the heat up. This delays start but hopefully means it works.
    }
    isCool = false;
    isOn = true;
    systemState = stateHeating;
  } else {
    isOn = false;
    systemState = stateOff;
  }
  
  isZone1Open = digitalRead(zone1Pin) == LOW;
  isZone2Open = digitalRead(zone2Pin) == LOW;
  isZone3Open = digitalRead(zone3Pin) == LOW;
  
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
    Serial.println("Cool");
  } else {
    Serial.println("Heat");
  }
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
  if(enableSerial) {
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
  mySerial.write(output[0]);
  mySerial.write(output[1]);
  mySerial.write(output[2]);
  mySerial.write(output[3]);
  mySerial.write(output[4]);
  mySerial.write(output[5]);
  mySerial.write(output[6]);
  mySerial.write(output[7]);
}
