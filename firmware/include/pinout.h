#pragma once
#include <Arduino.h>

// ==============================================================================
// ESP32-S3-DevKitC-1 N16R8 PIN DEFINITIONS: PROJECT 2
// Title: Intelligent Bio-Acoustic Mosquito Vector Surveillance and Selective Trapping System
// Course: CSE 326 - Microprocessor, Assembly Language & Computer Interfacing Sessional
// ==============================================================================

// 1. INMP441 I2S MEMS Omnidirectional Digital Microphone
#define PIN_I2S_WS        4     // Word Select (LRCK)
#define PIN_I2S_SCK       5     // Continuous Serial Clock (BCLK)
#define PIN_I2S_SD        6     // Serial Data In (DIN)

// 2. Dual LM393 IR Optical Photodiode Transit Gates
#define PIN_IR_GATE_ENTRY 7     // Gate 1: Entrance funneled trigger & transit timer
#define PIN_IR_GATE_TRAP  15    // Gate 2: Internal collection net confirmation counter

// 3. I2C Bus for 1.3-inch SH1106 128x64 OLED Display
#define PIN_I2C_SDA       8
#define PIN_I2C_SCL       9
#define ADDR_OLED         0x3C

// 4. DHT22 Thermal Wingbeat Compensation Sensor
#define PIN_DHT22         16    // Temperature compensation for wingbeat pitch

// 5. High-Frequency Silent PWM Suction Fan (IRFZ44N MOSFET Driver)
#define PIN_FAN_PWM       17    // 25 kHz PWM (Prevents ultrasonic disturbance)

// 6. 3-Zone Classification Status LEDs
#define PIN_LED_GREEN     10    // Green: Housefly / Benign insect (>15ms transit, ~160Hz)
#define PIN_LED_YELLOW    11    // Yellow: Non-biting Male mosquito (~560-650Hz, fan off)
#define PIN_LED_RED       12    // Red: Female Vector (Aedes ~495Hz, Anopheles ~325Hz) -> FAN 100%!

// 7. Push Buttons for UI Control
#define PIN_BTN_PAGE      18    // Switch display pages (Species vs Statistics)
#define PIN_BTN_RESET     19    // Reset in-trap count register
