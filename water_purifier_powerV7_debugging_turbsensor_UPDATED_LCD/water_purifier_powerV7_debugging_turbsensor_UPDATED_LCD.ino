#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DFRobot_PH.h"
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);
DFRobot_PH ph;

/* ================= SENSOR PINS ================= */
#define TDS_PIN        34
#define PH_PIN         35
#define TURBIDITY_PIN  32
#define EEPROM_SIZE    64 

/* ================= TURBIDITY CONSTANTS ================= */
int clearValue = 4095;  // Sensor value in clear water
int dirtyValue = 1000;  // Sensor value in dirty water
int turbidityPercentage = 0;
String turbidityStatus = "CLEAR";

/* ================= SENSOR CONSTANTS ================= */
#define SCOUNT 30           
int tdsBuffer[SCOUNT];
int tdsBufferIndex = 0;
float temperature = 25.0;   
float voltagePH, phValue;   

/* ================= FLOAT SWITCHES ================= */
#define FLOAT1 14 
#define FLOAT2 12 
#define FLOAT3 4  
#define FLOAT4 5  
#define FLOAT5 15 

/* ================= RELAYS ================= */
#define RELAY_PUMP1 26 
#define RELAY_VALVE1 27 
#define RELAY_PUMP2 13 
#define RELAY_VALVE2 33 
#define RELAY_PUMP3  18//32
#define RELAY_PUMP4 23
#define RELAY_UV 25

/* ================= TIMING & LIMITS ================= */
const unsigned long STABLE_TIME = 15000;
const unsigned long UV_DELAY = 3000;
const unsigned long UV_TIME = 1200000;
const unsigned long LCD_INTERVAL = 600; 

float phLow = 6.5;
float phHigh = 8.5;
float tdsLimit = 600;
float turbLimit = 50.0; // 50% threshold for unsafe water

/* ================= SYSTEM STATES ================= */
enum State { FILL_TANK1, STABILIZE, MONITOR_QUALITY, REFILTER, TRANSFER_TANK2, UV_STERILIZE, RESERVOIR_FILL, STANDBY };
State systemState = FILL_TANK1;

unsigned long stateStart = 0;
float tdsValue = 0;
float turbVoltage = 0;

/* ================= HELPERS ================= */
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  return (iFilterLen & 1) > 0 ? bTab[(iFilterLen - 1) / 2] : (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}

float readAverage(int pin) {
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return sum / 20.0;
}

/* ================= SENSOR READ ================= */
void readSensors() {
  // TDS
  tdsBuffer[tdsBufferIndex] = analogRead(TDS_PIN);
  tdsBufferIndex++;
  if (tdsBufferIndex >= SCOUNT) tdsBufferIndex = 0;
  float tdsRawMedian = getMedianNum(tdsBuffer, SCOUNT);
  float tdsVoltage = tdsRawMedian * (3.3 / 4095.0); 
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensationVoltage = tdsVoltage / compensationCoefficient;
  tdsValue = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;

  // Turbidity Logic
  float turbRaw = readAverage(TURBIDITY_PIN);
  turbVoltage = turbRaw * (3.3 / 4095.0);
  turbidityPercentage = map((int)turbRaw, clearValue, dirtyValue, 0, 100);
  turbidityPercentage = constrain(turbidityPercentage, 0, 100);

  if (turbidityPercentage < 30) turbidityStatus = "CLEAR ";
  else if (turbidityPercentage < 70) turbidityStatus = "CLOUDY";
  else turbidityStatus = "DIRTY ";

  // pH
  float phRaw = readAverage(PH_PIN);
  voltagePH = phRaw * (3300.0 / 4095.0); 
  phValue = ph.readPH(voltagePH, temperature);
}

/* ================= LCD UPDATE ================= */
void updateLCD() {
  static unsigned long lastLCDUpdate = 0;
  if (millis() - lastLCDUpdate > LCD_INTERVAL) {
    // Row 0: PH and TDS
    lcd.setCursor(0, 0);
    lcd.print("PH:"); lcd.print(phValue, 1);
    lcd.print(" TDS:"); lcd.print((int)tdsValue);
    lcd.print(" PPM    "); 

    // Row 1: Turbidity % and Status
    lcd.setCursor(0, 1);
    lcd.print("TURB:"); lcd.print(turbidityPercentage);
    lcd.print(" NTU "); lcd.print(turbidityStatus);
    lcd.print("        "); // Clearing extra space

    lastLCDUpdate = millis();
  }
}

void printSerialData() {
    static unsigned long lastSerialPrint = 0;
    if (millis() - lastSerialPrint > 2000) {
        Serial.print("pH: "); Serial.print(phValue, 2);
        Serial.print(" | TDS: "); Serial.print(tdsValue, 0);
        Serial.print(" | Turb: "); Serial.print(turbidityPercentage);
        Serial.print("% | State: "); Serial.println(systemState);
        lastSerialPrint = millis();
    }
}

void allPumpsOff() {
  digitalWrite(RELAY_PUMP1, HIGH);
  digitalWrite(RELAY_PUMP2, HIGH);
  digitalWrite(RELAY_PUMP3, HIGH);
  digitalWrite(RELAY_PUMP4, HIGH);
  digitalWrite(RELAY_UV, HIGH);
  digitalWrite(RELAY_VALVE1, HIGH);
  digitalWrite(RELAY_VALVE2, HIGH);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); 
  EEPROM.begin(EEPROM_SIZE);
  ph.begin();

  lcd.init();
  lcd.backlight();

  pinMode(FLOAT1, INPUT_PULLUP);
  pinMode(FLOAT2, INPUT_PULLUP);
  pinMode(FLOAT3, INPUT_PULLUP);
  pinMode(FLOAT4, INPUT_PULLUP);
  pinMode(FLOAT5, INPUT_PULLUP);

  pinMode(RELAY_PUMP1, OUTPUT);
  pinMode(RELAY_PUMP2, OUTPUT);
  pinMode(RELAY_PUMP3, OUTPUT);
  pinMode(RELAY_PUMP4, OUTPUT);
  pinMode(RELAY_VALVE1, OUTPUT);
  pinMode(RELAY_VALVE2, OUTPUT);
  pinMode(RELAY_UV, OUTPUT);

  allPumpsOff();
   lcd.setCursor(0,0);
  lcd.print("SMART WATER SYSTEM");
  lcd.setCursor(0,1);
  lcd.print("SYSTEM STARTING...");
  delay(2000);
  lcd.clear();
}

void loop() {
  if (Serial.available() > 0) {
      allPumpsOff();
      ph.calibration(voltagePH, temperature);
      return;
  }

  static unsigned long lastBufferFill = 0;
  if (millis() - lastBufferFill > 50) {
      readSensors(); 
      lastBufferFill = millis();
  }

  updateLCD();
  printSerialData();

  bool tank1Full = digitalRead(FLOAT1) == LOW;
  bool tank1Low = digitalRead(FLOAT2) == LOW; 
  bool tank2Full = digitalRead(FLOAT3) == HIGH;
  bool tank2Low = digitalRead(FLOAT4) == LOW;
  bool reservoirFull = digitalRead(FLOAT5) == LOW;

  bool unsafe = (tdsValue > tdsLimit + 10) || (turbidityPercentage >= 50) || (phValue < phLow) || (phValue > phHigh);

  if (tank1Low && systemState != FILL_TANK1) {
    allPumpsOff();
    systemState = FILL_TANK1;
  }

  switch (systemState) {
    case FILL_TANK1:
      lcd.setCursor(0, 2); lcd.print("STATUS: FILLING T1  ");
      lcd.setCursor(0, 3); lcd.print("PUMP1 & VALVE1 ON   ");
      digitalWrite(RELAY_PUMP1, LOW);
      digitalWrite(RELAY_VALVE1, LOW);
      if (tank1Full) {
        digitalWrite(RELAY_PUMP1, HIGH);
        digitalWrite(RELAY_VALVE1, HIGH);
        systemState = STABILIZE;
        stateStart = millis();
      }
      break;

    case STABILIZE:
      lcd.setCursor(0, 2); lcd.print("STATUS: STABILIZING ");
      lcd.setCursor(0, 3); lcd.print("WAITING FOR SENSORS ");
      if (millis() - stateStart > STABLE_TIME) {
        systemState = MONITOR_QUALITY;
      }
      break;

    case MONITOR_QUALITY:
      lcd.setCursor(0, 2); lcd.print("STATUS: CHECKING    ");
      if (unsafe) {
          lcd.setCursor(0, 3); lcd.print("QUALITY: UNSAFE!    ");
          systemState = REFILTER;
      } else {
          lcd.setCursor(0, 3); lcd.print("QUALITY: SAFE       ");
          systemState = TRANSFER_TANK2;
      }
      break;

    case REFILTER:
      lcd.setCursor(0, 2); lcd.print("STATUS: REFILTERING ");
      lcd.setCursor(0, 3); lcd.print("PUMP2 & VALVE2 ON   ");
      digitalWrite(RELAY_PUMP2, LOW);
      digitalWrite(RELAY_VALVE2, LOW);
      if (!unsafe) {
        digitalWrite(RELAY_PUMP2, HIGH);
        digitalWrite(RELAY_VALVE2, HIGH);
        systemState = MONITOR_QUALITY;
      }
      break;

    case TRANSFER_TANK2:
      lcd.setCursor(0, 2); lcd.print("STATUS: TRANSFERRING");
      lcd.setCursor(0, 3); lcd.print("PUMP 3 (T1 -> T2)   ");
      if (!tank2Full) {
        digitalWrite(RELAY_PUMP3, LOW);
      } else {
        digitalWrite(RELAY_PUMP3, HIGH);
        systemState = UV_STERILIZE;
        stateStart = millis();
      }
      break;

    case UV_STERILIZE:
      lcd.setCursor(0, 2); lcd.print("STATUS: UV SCAN     ");
      lcd.setCursor(0, 3); lcd.print("UV LAMP ACTIVE      ");
      if (millis() - stateStart > UV_DELAY) digitalWrite(RELAY_UV, LOW);
      if (millis() - stateStart > UV_TIME) {
        digitalWrite(RELAY_UV, HIGH);
        systemState = RESERVOIR_FILL;
      }
      break;

    case RESERVOIR_FILL:
      lcd.setCursor(0, 2); lcd.print("STATUS: RES. FILL   ");
      lcd.setCursor(0, 3); lcd.print("PUMP 4 ACTIVE       ");
      if (!reservoirFull && !tank2Low) {
        digitalWrite(RELAY_PUMP4, LOW);
      } else {
        digitalWrite(RELAY_PUMP4, HIGH);
        systemState = STANDBY;
      }
      break;

    case STANDBY:
      lcd.setCursor(0, 2); lcd.print("STATUS: STANDBY     ");
      lcd.setCursor(0, 3); lcd.print("SYSTEM READY        ");
      if ((!reservoirFull || tank2Low) && !tank1Low) {
        systemState = TRANSFER_TANK2;
      }
      break;
  }
}