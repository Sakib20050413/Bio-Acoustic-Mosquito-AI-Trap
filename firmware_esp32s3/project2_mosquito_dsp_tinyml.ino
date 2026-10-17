// =============================================================================
// PROJECT 2 (CSE 326): BIO-ACOUSTIC MOSQUITO VECTOR DSP & SELECTIVE TRAP
// Target MCU: ESP32-S3-DevKitC-1 N16R8 Dual Type-C (44-Pin)
// Team BlackBerry (ID: 1119003, 1119004, 1119007) — BAIUST, Cumilla
// =============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>

#define I2S_WS_PIN        4
#define I2S_SCK_PIN       5
#define I2S_SD_PIN        6
#define IR_ENTRY_GATE_PIN 7
#define IR_TRAP_GATE_PIN  15
#define OLED_SDA_PIN      8
#define OLED_SCL_PIN      9
#define DHT22_PIN         16
#define FAN_PWM_PIN       17
#define BTN_PAGE_PIN      18
#define BTN_RESET_PIN     19
#define LED_GREEN_PIN     10
#define LED_YELLOW_PIN    11
#define LED_RED_PIN       12

volatile uint32_t gateStartUs = 0;
volatile float lastTransitMs = 0.0;
volatile uint32_t inTrapCount = 0;

void IRAM_ATTR onEntryGateISR() {
  if (digitalRead(IR_ENTRY_GATE_PIN) == LOW) {
    gateStartUs = micros();
  } else {
    lastTransitMs = (micros() - gateStartUs) / 1000.0;
  }
}

void IRAM_ATTR onTrapGateISR() {
  inTrapCount++;
}

void classifyWingbeatAndActuate(float measuredHz, float transitMs, float ambientTempC) {
  // Temperature compensation to 27.0 C reference
  float correctedHz = measuredHz - 7.5 * (ambientTempC - 27.0);

  if (transitMs > 12.0 || (correctedHz >= 80 && correctedHz <= 250)) {
    // Housefly -> Green LED, Fan stays at 20% silent standby
    digitalWrite(LED_GREEN_PIN, HIGH);
    digitalWrite(LED_YELLOW_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);
    analogWrite(FAN_PWM_PIN, 50);
  } else if (correctedHz >= 295 && correctedHz <= 580 && transitMs <= 3.8) {
    // Female Vector (Anopheles / Culex / Aedes) -> Red LED + 100% PWM Suction Fan!
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_YELLOW_PIN, LOW);
    digitalWrite(LED_RED_PIN, HIGH);
    analogWrite(FAN_PWM_PIN, 255);
  } else {
    // Non-biting Male -> Yellow LED
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_YELLOW_PIN, HIGH);
    digitalWrite(LED_RED_PIN, LOW);
    analogWrite(FAN_PWM_PIN, 50);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  pinMode(IR_ENTRY_GATE_PIN, INPUT_PULLUP);
  pinMode(IR_TRAP_GATE_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(FAN_PWM_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(IR_ENTRY_GATE_PIN), onEntryGateISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IR_TRAP_GATE_PIN), onTrapGateISR, FALLING);
  Serial.println("BAIUST CSE-326 ESP32-S3 Bio-Acoustic Mosquito Trap Ready.");
}

void loop() {
  delay(100);
}
