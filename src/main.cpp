#include <Arduino.h>
#include <AccelStepper.h>

// ---------------------------
// Hardware Definitionen
// ---------------------------
const int stepPin = 2;
const int dirPin = 3;
const int enPin = 4;
const int minSwitchPin = 5;
const int maxSwitchPin = 6;

// TMC2209 UART Pins für Hardware UART1 des Pico
// UART1: TX = GPIO4 (Pin 6), RX = GPIO5 (Pin 7)

// ---------------------------
// Globale Variablen
// ---------------------------
AccelStepper stepper(AccelStepper::DRIVER, stepPin, dirPin);
long currentPosition = 0;
long maxPosition = 0;
long targetPosition = 0;

// Soft Endstops - konfigurierbar
long softMinPosition = 0;      // Standard: 0
long softMaxPosition = 0;      // Standard: wird nach Homing auf maxPosition gesetzt
bool softEndstopsEnabled = true; // Soft Endstops aktiviert/deaktiviert

enum State {
  INIT,
  HOMING_MIN,
  HOMING_MAX,
  READY,
  ERROR_DETECTED,
  WAIT_RESET,
  AUTO_REHOME
};

State currentState = INIT;
unsigned long errorTimestamp = 0;
const unsigned long resetTimeout = 10 * 60 * 1000UL;
bool stallDetected = false;
const int SG_THRESHOLD = 100; // Schwellwert für StallGuard Auslösung (anpassen je nach Motorlast!)

// ---------------------------
// Funktionsdeklarationen
// ---------------------------
void handleSerial();
bool checkForUnexpectedSwitch();
void errorDetected();
void resetMotor();
void initTMC2209();
void sendTMC2209Command(byte address, byte value);
void checkStallGuard();
bool checkSoftEndstops(long position);
void setSoftEndstops(long minPos, long maxPos);
void moveTo(long position);

// ---------------------------
// Setup
// ---------------------------
void setup() {
  Serial.begin(115200);
  pinMode(enPin, OUTPUT);
  digitalWrite(enPin, LOW);
  pinMode(minSwitchPin, INPUT_PULLUP);
  pinMode(maxSwitchPin, INPUT_PULLUP);
  stepper.setMaxSpeed(400);
  stepper.setAcceleration(1000);
  
  // Hardware UART für TMC2209
  Serial1.begin(115200);
  
  Serial.println("System gestartet...");
  initTMC2209(); // StallGuard aktivieren!
}

// ---------------------------
// Haupt-Loop
// ---------------------------
void loop() {
  handleSerial();
  checkStallGuard();
  
  switch (currentState) {
    case INIT:
      Serial.println("Starte Homing...");
      stepper.setMaxSpeed(200);
      stepper.moveTo(-100000);
      currentState = HOMING_MIN;
      break;
      
    case HOMING_MIN:
      stepper.run();
      if (digitalRead(minSwitchPin) == LOW) {
        stepper.stop();
        stepper.setCurrentPosition(0);
        Serial.println("Min-Position erreicht.");
        delay(500);
        stepper.moveTo(100000);
        currentState = HOMING_MAX;
      }
      break;
      
    case HOMING_MAX:
      stepper.run();
      if (digitalRead(maxSwitchPin) == LOW) {
        stepper.stop();
        maxPosition = stepper.currentPosition();
        Serial.print("Max-Position erreicht bei Steps: ");
        Serial.println(maxPosition);
        
        // Setze Soft Endstops nach Homing
        softMinPosition = 0;
        softMaxPosition = maxPosition;
        Serial.println("Soft Endstops gesetzt: Min=0, Max=" + String(maxPosition));
        
        delay(500);
        moveTo(maxPosition / 2);
        targetPosition = maxPosition / 2;
        currentState = READY;
      }
      break;
      
    case READY:
      stepper.run();
      if (checkForUnexpectedSwitch() || stallDetected) {
        errorDetected();
      }
      break;
      
    case ERROR_DETECTED:
      stepper.stop();
      digitalWrite(enPin, HIGH);
      Serial.println("FEHLER: Motor gestoppt! Warte auf RESET...");
      errorTimestamp = millis();
      currentState = WAIT_RESET;
      break;
      
    case WAIT_RESET:
      if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "RESET") {
          Serial.println("RESET empfangen! Starte Homing neu...");
          resetMotor();
        }
      }
      if (millis() - errorTimestamp >= resetTimeout) {
        Serial.println("10 Minuten vorbei - automatisches Re-Homing...");
        resetMotor();
      }
      break;
      
    case AUTO_REHOME:
      digitalWrite(enPin, LOW);
      stepper.setMaxSpeed(200);
      stepper.moveTo(-100000);
      currentState = HOMING_MIN;
      break;
  }
}

// ---------------------------
// Soft Endstop Funktionen
// ---------------------------
bool checkSoftEndstops(long position) {
  if (!softEndstopsEnabled) return true;
  
  if (position < softMinPosition) {
    Serial.println("Soft Min Endstop erreicht! Position begrenzt auf " + String(softMinPosition));
    return false;
  }
  
  if (position > softMaxPosition) {
    Serial.println("Soft Max Endstop erreicht! Position begrenzt auf " + String(softMaxPosition));
    return false;
  }
  
  return true;
}

void setSoftEndstops(long minPos, long maxPos) {
  if (minPos < 0) minPos = 0;
  if (maxPos > maxPosition) maxPos = maxPosition;
  if (minPos >= maxPos) {
    Serial.println("Fehler: Min muss kleiner als Max sein!");
    return;
  }
  
  softMinPosition = minPos;
  softMaxPosition = maxPos;
  Serial.println("Soft Endstops gesetzt: Min=" + String(minPos) + ", Max=" + String(maxPos));
}

void moveTo(long position) {
  if (checkSoftEndstops(position)) {
    stepper.moveTo(position);
    targetPosition = position;
  } else {
    // Wenn Position außerhalb der Soft Endstops liegt, bewege zur nächsten erlaubten Position
    if (position < softMinPosition) {
      stepper.moveTo(softMinPosition);
      targetPosition = softMinPosition;
    } else if (position > softMaxPosition) {
      stepper.moveTo(softMaxPosition);
      targetPosition = softMaxPosition;
    }
  }
}

// ---------------------------
// Hilfsfunktionen
// ---------------------------
void handleSerial() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.startsWith("GOTO")) {
      if (currentState == READY) {
        long pos = input.substring(5).toInt();
        moveTo(pos);
        Serial.print("Bewege zu Position: ");
        Serial.println(targetPosition);
      } else {
        Serial.println("Motor nicht bereit.");
      }
    }
    else if (input == "POS?") {
      Serial.print("Aktuelle Position: ");
      Serial.println(stepper.currentPosition());
    }
    else if (input == "RESET") {
      if (currentState == WAIT_RESET) {
        Serial.println("RESET während WAIT_RESET empfangen.");
      } else {
        Serial.println("RESET ignoriert (kein Fehler aktiv).");
      }
    }
    else if (input == "HOME") {
      Serial.println("Manuelles Homing gestartet...");
      resetMotor();
    }
    else if (input.startsWith("SOFTMIN")) {
      if (currentState == READY) {
        long minPos = input.substring(8).toInt();
        setSoftEndstops(minPos, softMaxPosition);
      } else {
        Serial.println("Motor nicht bereit.");
      }
    }
    else if (input.startsWith("SOFTMAX")) {
      if (currentState == READY) {
        long maxPos = input.substring(8).toInt();
        setSoftEndstops(softMinPosition, maxPos);
      } else {
        Serial.println("Motor nicht bereit.");
      }
    }
    else if (input == "SOFTENDSTOPS ON") {
      softEndstopsEnabled = true;
      Serial.println("Soft Endstops aktiviert");
    }
    else if (input == "SOFTENDSTOPS OFF") {
      softEndstopsEnabled = false;
      Serial.println("Soft Endstops deaktiviert");
    }
    else if (input == "SOFTENDSTOPS?") {
      Serial.println("Soft Endstops: " + String(softEndstopsEnabled ? "aktiviert" : "deaktiviert"));
      Serial.println("Min: " + String(softMinPosition) + ", Max: " + String(softMaxPosition));
    }
    else {
      Serial.println("Unbekannter Befehl.");
    }
  }
}

bool checkForUnexpectedSwitch() {
  if (digitalRead(minSwitchPin) == LOW && stepper.speed() > 0) return true;
  if (digitalRead(maxSwitchPin) == LOW && stepper.speed() < 0) return true;
  return false;
}

void errorDetected() {
  stepper.stop();
  digitalWrite(enPin, HIGH);
  currentState = ERROR_DETECTED;
}

void resetMotor() {
  stallDetected = false;
  currentState = AUTO_REHOME;
}

// ---------------------------
// TMC2209 Initialisierung
// ---------------------------
void initTMC2209() {
  Serial.println("Initialisiere TMC2209...");
  sendTMC2209Command(0x03, 0x00); // Reset IC (GCONF)
  delay(10);
  
  // CoolStep deaktivieren, StallGuard aktivieren
  sendTMC2209Command(0x6C, 0x00); // CoolStep abschalten
  
  // StallGuard Threshold setzen (niedriger Wert = empfindlicher)
  sendTMC2209Command(0x40, SG_THRESHOLD);
  
  Serial.println("TMC2209 konfiguriert.");
}

void sendTMC2209Command(byte address, byte value) {
  Serial1.write(0x05); // Sync
  Serial1.write(address); // Registeradresse
  Serial1.write(value); // Wert
  Serial1.flush();
}

void checkStallGuard() {
  if (currentState != READY) return;
  
  Serial1.write(0x05); // Sync
  Serial1.write(0x6F); // Request DRV_STATUS
  Serial1.flush();
  
  if (Serial1.available() > 0) {
    byte status = Serial1.read();
    if ((status & 0x10) == 0x10) { // StallBit gesetzt
      stallDetected = true;
      Serial.println("Stall erkannt!");
    }
  }
}
