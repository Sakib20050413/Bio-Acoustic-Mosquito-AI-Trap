#include <Arduino.h>
#include <Wire.h>
#include "pinout.h"
#include "i2s_audio.h"
#include "tinyml_classifier.h"

I2SAudioDriver audio;
TinyMLMosquitoClassifier classifier;

// Statistics Registers
volatile uint32_t countFlies = 0;
volatile uint32_t countMales = 0;
volatile uint32_t countVectorsTrapped = 0;

// Optical Gate Transit Timing
volatile unsigned long gate1TriggerTime = 0;
volatile float lastTransitDurationMs = 2.4f; // default mosquito body transit

void IRAM_ATTR onEntryGateInterrupt() {
    gate1TriggerTime = micros();
}

void IRAM_ATTR onTrapGateInterrupt() {
    // Mosquito reached inside collection chamber
    countVectorsTrapped++;
}

void setFanSpeed(uint8_t duty) {
    ledcWrite(1, duty); // 0 = 0%, 255 = 100%
}

void setClassificationLeds(InsectClassification species) {
    digitalWrite(PIN_LED_GREEN, species == CLASS_HOUSEFLY ? HIGH : LOW);
    digitalWrite(PIN_LED_YELLOW, species == CLASS_MALE_MOSQUITO ? HIGH : LOW);
    digitalWrite(PIN_LED_RED, (species == CLASS_FEMALE_AEDES || 
                               species == CLASS_FEMALE_ANOPHELES || 
                               species == CLASS_FEMALE_CULEX) ? HIGH : LOW);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("======================================================"));
    Serial.println(F("BAIUST CSE 326 - Microprocessor & Interfacing Sessional"));
    Serial.println(F("ESP32-S3 Bio-Acoustic Mosquito Vector AI Trap"));
    Serial.println(F("Team BlackBerry: Sakib (1119003), Ifag, Towhid"));
    Serial.println(F("======================================================"));

    // 1. Setup LEDs & Optical Gates
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    setClassificationLeds(CLASS_NONE);

    pinMode(PIN_IR_GATE_ENTRY, INPUT);
    pinMode(PIN_IR_GATE_TRAP, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_IR_GATE_ENTRY), onEntryGateInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_IR_GATE_TRAP), onTrapGateInterrupt, FALLING);

    pinMode(PIN_BTN_PAGE, INPUT_PULLUP);
    pinMode(PIN_BTN_RESET, INPUT_PULLUP);

    // 2. Setup 25 kHz Silent PWM Fan via IRFZ44N MOSFET
    ledcSetup(1, 25000, 8); // Channel 1, 25kHz, 8-bit
    ledcAttachPin(PIN_FAN_PWM, 1);
    setFanSpeed(0); // Fan idle / OFF

    // 3. Initialize I2S Audio Driver
    if (audio.begin()) {
        Serial.println(F("[I2S] INMP441 Microphone Initialized at 16 kHz."));
    } else {
        Serial.println(F("[I2S ERROR] Failed to initialize I2S interface."));
    }
}

unsigned long lastStatusPrint = 0;
float currentSimFreq = 0.0f;
float ambientTempC = 22.0f; // Winter room temperature

void loop() {
    // Check reset button
    if (digitalRead(PIN_BTN_RESET) == LOW) {
        countFlies = 0;
        countMales = 0;
        countVectorsTrapped = 0;
        Serial.println(F("[UI] Trap Statistics Reset to Zero."));
        delay(300);
    }

    // Audio frame capture
    audio.captureAudioFrame();

    // Check serial simulation commands for Winter Lab Final Viva
    if (Serial.available()) {
        char c = Serial.read();
        if (c == '1') {
            // Test 1: Housefly (160 Hz, slow 16ms transit)
            currentSimFreq = 160.0f;
            lastTransitDurationMs = 16.5f;
            countFlies++;
            Serial.println(F("[TEST] HumBugDB: Housefly (160 Hz, 16.5ms transit)"));
        } else if (c == '2') {
            // Test 2: Male Culex (560 Hz, fast 2.2ms transit)
            currentSimFreq = 560.0f;
            lastTransitDurationMs = 2.2f;
            countMales++;
            Serial.println(F("[TEST] HumBugDB: Male Culex (560 Hz, Non-Biting)"));
        } else if (c == '3') {
            // Test 3: Female Aedes aegypti (Dengue, 495 Hz, fast 2.0ms transit)
            currentSimFreq = 495.0f;
            lastTransitDurationMs = 2.0f;
            Serial.println(F("[TEST] HumBugDB: Female Aedes aegypti (495 Hz, Dengue Vector) -> TRAP!"));
        } else if (c == '4') {
            // Test 4: Female Anopheles stephensi (Malaria, 330 Hz, fast 2.5ms transit)
            currentSimFreq = 330.0f;
            lastTransitDurationMs = 2.5f;
            Serial.println(F("[TEST] HumBugDB: Female Anopheles (330 Hz, Malaria Vector) -> TRAP!"));
        } else if (c == '0') {
            currentSimFreq = 0.0f;
            setFanSpeed(0);
            setClassificationLeds(CLASS_NONE);
            Serial.println(F("[TEST] Idle / Silence."));
        }
    }

    if (currentSimFreq > 50.0f) {
        ClassificationResult res = classifier.classify(currentSimFreq, lastTransitDurationMs, ambientTempC);
        setClassificationLeds(res.species);

        if (res.shouldTriggerSuctionFan) {
            // Full 100% suction to capture mosquito into collection net
            setFanSpeed(255);
            countVectorsTrapped++;
            Serial.print(F("[ACTION] FAN 100% ACTIVE! Trapped Vector: "));
            Serial.print(res.species == CLASS_FEMALE_AEDES ? "Aedes aegypti (Dengue)" : "Anopheles (Malaria)");
            Serial.print(F(" | Freq: ")); Serial.print(res.fundamentalFreqHz);
            Serial.println(F(" Hz"));
            delay(1200); // Suction pull duration
            setFanSpeed(0);
            currentSimFreq = 0.0f; // reset trigger
        } else {
            setFanSpeed(0);
            if (res.species == CLASS_HOUSEFLY) {
                Serial.println(F("[ACTION] Ignored: Non-target Housefly (Fan OFF, Green LED)"));
            } else if (res.species == CLASS_MALE_MOSQUITO) {
                Serial.println(F("[ACTION] Ignored: Non-biting Male Mosquito (Fan OFF, Yellow LED)"));
            }
            delay(800);
            currentSimFreq = 0.0f;
        }
    }

    // Telemetry log output
    if (millis() - lastStatusPrint > 2500) {
        lastStatusPrint = millis();
        Serial.print(F("MOSQUITO_TRAP_LOG,Flies:"));
        Serial.print(countFlies);
        Serial.print(F(",Males:"));
        Serial.print(countMales);
        Serial.print(F(",TrappedVectors:"));
        Serial.println(countVectorsTrapped);
    }
}
