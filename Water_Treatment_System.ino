#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Sensor pins
#define turbidityPin A2
#define TRIGGER_PIN 6
#define ECHO_PIN 5

// Relay pins (active-low: LOW = ON, HIGH = OFF)
#define pumpPin 8
#define valveOutPin 11
#define valve2Pin 10
#define valve1Pin 9
#define chemicalPump1Pin 12
#define chemicalPump2Pin 7
#define mixerPin 13

// LED indicator pins
#define GREEN_LED 2
#define YELLOW_LED 3
#define RED_LED 4

LiquidCrystal_I2C lcd(0x27, 20, 4);  // LCD configuration

// Timing constants (milliseconds with UL suffix)
#define CYCLE_DURATION        810000UL    // 13.5 minutes
#define VALVE1_START          480000UL    // 8 minutes
#define VALVE1_END            540000UL    // 9 minutes
#define CHEM_PUMP1_START      540000UL    // 9 minutes
#define CHEM_PUMP1_END        555000UL    // 9.25 minutes
#define CHEM_PUMP2_START      570000UL    // 9.5 minutes
#define CHEM_PUMP2_END        585000UL    // 9.75 minutes
#define MIXER_START           540000UL    // 9 minutes
#define MIXER_END             630000UL    // 10.5 minutes
#define VALVE2_START          630000UL    // 10.5 minutes
#define VALVE2_END            690000UL    // 11.5 minutes
#define CONDITIONAL_START     750000UL    // 12.5 minutes
#define CONDITIONAL_END       810000UL    // 13.5 minutes

// System state variables
unsigned long cycleStartTime = 0;
unsigned long previousMillis = 0;
const long updateInterval = 500;  // Update every 500ms
int cycleCount = 0;

// State flags
bool valve1Active = false;
bool valve2Active = false;
bool pumpActive = false;
bool mixerActive = false;
bool chemPump1Active = false;
bool chemPump2Active = false;
bool valveOutActive = false;

void resetAllRelays() {
  digitalWrite(valve1Pin, HIGH);
  digitalWrite(valve2Pin, HIGH);
  digitalWrite(valveOutPin, HIGH);
  digitalWrite(chemicalPump1Pin, HIGH);
  digitalWrite(chemicalPump2Pin, HIGH);
  digitalWrite(mixerPin, HIGH);
  digitalWrite(pumpPin, HIGH);

  valve1Active = false;
  valve2Active = false;
  valveOutActive = false;
  chemPump1Active = false;
  chemPump2Active = false;
  mixerActive = false;
  pumpActive = false;
}

float readTurbidityNTU() {
  int sensorValue = analogRead(turbidityPin);
  float voltage = sensorValue * (5.0 / 1023.0);
  float ntu = 3000.0 - (voltage * 1000.0);  // Approximation
  return ntu;
}

String getTurbidityStatus(float ntu) {
  if (ntu <= 5.0) return "Clear";
  else if (ntu <= 15.0) return "Cloudy";
  else return "Dirty";
}

void updateTurbidityLEDs(String status) {
  digitalWrite(GREEN_LED, status == "Clear");
  digitalWrite(YELLOW_LED, status == "Cloudy");
  digitalWrite(RED_LED, status == "Dirty");
}

float readWaterLevel() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;  // Distance in cm
}

void updateDisplay(unsigned long elapsedTime, float turbidityNTU, String turbidityStatus, float waterLevel) {
  int totalSeconds = elapsedTime / 1000;
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cycle "); lcd.print(min(cycleCount + 1, 5)); lcd.print("/5 ");
  lcd.setCursor(12, 0); lcd.print(minutes); lcd.print("m "); lcd.print(seconds); lcd.print("s");

  lcd.setCursor(0, 1);
  lcd.print("Turb: "); lcd.print(turbidityNTU, 1); lcd.print(" NTU "); lcd.print(turbidityStatus);

  lcd.setCursor(0, 2);
  lcd.print("Level: "); lcd.print(waterLevel, 1); lcd.print(" cm");

  lcd.setCursor(0, 3);
  if (cycleCount < 5) {
    if (elapsedTime < VALVE1_START) lcd.print("Stage 1: Preparation");
    else if (elapsedTime < VALVE2_END) lcd.print("Stage 2: Treatment");
    else if (elapsedTime < CONDITIONAL_START) lcd.print("Stage 3: Settling");
    else lcd.print("Stage 4: Draining");
  } else {
    lcd.print("Quality Check Mode");
  }
}

void controlSystem(unsigned long elapsedTime, String turbidityStatus) {
  // Valve 1
  if (elapsedTime >= VALVE1_START && elapsedTime < VALVE1_END && !valve1Active) {
    digitalWrite(valve1Pin, LOW); valve1Active = true; Serial.println("Valve 1 ON");
  } else if (elapsedTime >= VALVE1_END && valve1Active) {
    digitalWrite(valve1Pin, HIGH); valve1Active = false; Serial.println("Valve 1 OFF");
  }

  // Chemical Pump 1
  if (elapsedTime >= CHEM_PUMP1_START && elapsedTime < CHEM_PUMP1_END && !chemPump1Active) {
    digitalWrite(chemicalPump1Pin, LOW); chemPump1Active = true; Serial.println("Chem Pump 1 ON");
  } else if (elapsedTime >= CHEM_PUMP1_END && chemPump1Active) {
    digitalWrite(chemicalPump1Pin, HIGH); chemPump1Active = false; Serial.println("Chem Pump 1 OFF");
  }

  // Chemical Pump 2
  if (elapsedTime >= CHEM_PUMP2_START && elapsedTime < CHEM_PUMP2_END && !chemPump2Active) {
    digitalWrite(chemicalPump2Pin, LOW); chemPump2Active = true; Serial.println("Chem Pump 2 ON");
  } else if (elapsedTime >= CHEM_PUMP2_END && chemPump2Active) {
    digitalWrite(chemicalPump2Pin, HIGH); chemPump2Active = false; Serial.println("Chem Pump 2 OFF");
  }

  // Mixer
  if (elapsedTime >= MIXER_START && elapsedTime < MIXER_END && !mixerActive) {
    digitalWrite(mixerPin, LOW); mixerActive = true; Serial.println("Mixer ON");
  } else if (elapsedTime >= MIXER_END && mixerActive) {
    digitalWrite(mixerPin, HIGH); mixerActive = false; Serial.println("Mixer OFF");
  }

  // Valve 2
  if (elapsedTime >= VALVE2_START && elapsedTime < VALVE2_END && !valve2Active) {
    digitalWrite(valve2Pin, LOW); valve2Active = true; Serial.println("Valve 2 ON");
  } else if (elapsedTime >= VALVE2_END && valve2Active) {
    digitalWrite(valve2Pin, HIGH); valve2Active = false; Serial.println("Valve 2 OFF");
  }

  // Conditional stage (Clear → ValveOut, Turbid → Pump)
  if (elapsedTime >= CONDITIONAL_START && elapsedTime < CONDITIONAL_END) {
    if (turbidityStatus == "Clear") {
      if (!valveOutActive) {
        digitalWrite(valveOutPin, LOW); 
        digitalWrite(pumpPin, HIGH);
        valveOutActive = true; pumpActive = false;
        Serial.println("Valve Out ON (Clear)");
      }
    } else {
      if (!pumpActive) {
        digitalWrite(pumpPin, LOW); 
        digitalWrite(valveOutPin, HIGH);
        pumpActive = true; valveOutActive = false;
        Serial.println("Pump ON (Turbid)");
      }
    }
  } else if (elapsedTime >= CONDITIONAL_END) {
    if (pumpActive) {
      digitalWrite(pumpPin, HIGH); pumpActive = false; Serial.println("Pump OFF");
    }
    if (valveOutActive) {
      digitalWrite(valveOutPin, HIGH); valveOutActive = false; Serial.println("Valve Out OFF");
    }
  }
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // Pin modes
  pinMode(pumpPin, OUTPUT);
  pinMode(valveOutPin, OUTPUT);
  pinMode(valve2Pin, OUTPUT);
  pinMode(valve1Pin, OUTPUT);
  pinMode(chemicalPump1Pin, OUTPUT);
  pinMode(chemicalPump2Pin, OUTPUT);
  pinMode(mixerPin, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  resetAllRelays();
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  cycleStartTime = millis();
  Serial.println("System initialized. Starting first cycle.");
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long elapsedTime = currentMillis - cycleStartTime;

  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;

    float turbidityNTU = readTurbidityNTU();
    String turbidityStatus = getTurbidityStatus(turbidityNTU);
    float waterLevel = readWaterLevel();

    updateTurbidityLEDs(turbidityStatus);
    updateDisplay(elapsedTime, turbidityNTU, turbidityStatus, waterLevel);

    if (cycleCount < 5) {
      controlSystem(elapsedTime, turbidityStatus);

      if (elapsedTime >= CYCLE_DURATION) {
        cycleCount++;
        cycleStartTime = currentMillis;
        resetAllRelays();
        Serial.print("Completed cycle ");
        Serial.print(cycleCount);
        Serial.println("/5");
        
        if (cycleCount >= 5) {
          Serial.println("All 5 cycles completed. Entering quality check.");
        }
      }
    } else {
      // Quality check after 5 cycles
      if (turbidityStatus == "Clear" && waterLevel > 10.0) {
        digitalWrite(valveOutPin, LOW);  // Open output valve
        digitalWrite(pumpPin, HIGH);    // Ensure pump is off
        lcd.setCursor(15, 3); lcd.print("OK  ");
      } else {
        digitalWrite(valveOutPin, HIGH); // Close output valve
        digitalWrite(pumpPin, HIGH);     // Ensure pump is off
        lcd.setCursor(15, 3); lcd.print("FAIL");
      }
    }
  }
}
