# PROJECT 2 (CSE 326 - SESSIONAL) — ANTIGRAVITY WORKSPACE CONTEXT

> **STRICT CONTEXT LOCK: PROJECT 2 ONLY**
> This workspace is exclusively for **CSE 326 Microprocessor / Embedded Systems Sessional** at **BAIUST, Cumilla, Bangladesh**.
> **NEVER** mix Project 1 (Storm & Lightning Box / CSE 301) pins, code, or context into this project.

## 1. Team, Supervisor & Language
- **Title:** Bio-Acoustic and Optical Mosquito Trap Using ESP32-S3 512-Point FFT and On-Box OLED Band Counters (`WingBeat-TinyNet`)
- **Team:** Md. Najmus Sakib Rahatul (`1119003`), Nesar Uddin Ifag (`1119004`), Rayhan Alam Towhid (`1119007`)
- **Supervisor:** Md. Fahim Kabir Chowdhury (Lecturer, Dept. of CSE, BAIUST)
- **Response Language:** Always respond in **Bengali script (বাংলা)** with technical terms, code, CLI commands, and pin names in English (unless explicitly asked for English, e.g., for Q1 paper / report text).

## 2. Verified Physical Hardware & Exact Pinout (`ESP32-S3 N16R8`, 44-Pin)
All 12 hardware groups (Items 22–33, 2,747 BDT) are physically owned and photo-verified:
- **MCU:** `ESP32-S3 N16R8` (44-Pin, `5Vin` pin; held upright with metal shield up: **Right USB-C = `UART`**, **Left USB-C = `USB`**)
- **3.3V Safe Sensor Rail (`3V3`):**
  - `INMP441` Round I2S MEMS Mic: `SCK=GPIO 4`, `WS=GPIO 5`, `SD=GPIO 6`, `L/R=GND`, `VDD=3.3V`, `GND=GND` (NEVER 5V!)
  - `DHT22` (3-Pin `V182` PCB with built-in pull-up): `+=3.3V`, `out=GPIO 7`, `-=GND` — thermal normalization: `f0_corr = f0_raw - 7.0 * (T_amb - 28.0)`
  - `1.3" SH1106 OLED` (`DST-013`, I2C `0x3C`): `GND=GND`, `VCC=3.3V`, `SCL=GPIO 9`, `SDA=GPIO 8`
  - `2x HW-201 / FC-51` LM393 IR Gates (left-to-right `OUT | GND | VCC`): Gate #1 `OUT=GPIO 10` (Entry `dt_ms`), Gate #2 `OUT=GPIO 14` (Mesh catch), `VCC=3.3V`
- **5.0V Actuator Rail (`5Vin`):**
  - `5V 1-Channel Relay` (`JQC3F-05VDC-C`, **NO MOSFET**): Logic `VCC=5Vin`, `GND=GND`, `IN=GPIO 13`; Load screws `COM=5Vin`, `NO=Fan Red(+)`, `NC=Empty`
  - `5V Brushless Fan` (`JD-4007S5L2`): `Red(+)=Relay NO`, `Black(-)=GND`
  - Passive Lure Ring (optional during bench test): `6x 395nm UV LEDs` (`220 Ohm` each) + `100 Ohm 5W` Ceramic Resistor across `5Vin` & `GND` (`0.25W` / `+2.5°C` at `5V`)
- **I/O:**
  - `3x 10mm Status LEDs` (`+220 Ohm` each, short leg to `GND`): Green=`GPIO 40`, Yellow=`GPIO 41`, Red=`GPIO 42`
  - `4x Yellow Tactile Buttons` (straddling breadboard center groove, diagonal legs): `GPIO 38, 39, 1, 2` to `GND`
- **Power & Breadboard:** `2x MB-102` breadboards (NOTE: side `+`/`-` rails split at row 30 — bridge or keep modules on same half), `20W` Power Bank (`USB1/OUT` or `USB2/OUT` 5V).

## 3. Arduino IDE 1.8.19 (Ubuntu Linux) Confirmed Configuration
- **Board Core:** `esp32 by Espressif Systems` **`v2.0.17`** (strictly kept over v3.x for legacy `driver/i2s.h` compatibility; zero performance penalty)
- **Tools Menu:** `Board: ESP32S3 Dev Module`, `USB CDC On Boot: Enabled`, `Upload Speed: 115200`, `Flash Size: 16MB (128Mb)`, `PSRAM: OPI PSRAM`, `Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)`
- **Libraries:** `arduinoFFT` (v2.x), `DHT sensor library` (Adafruit), `Adafruit Unified Sensor`, `U8g2` (oliver)

## 4. Strict Q1 Publication & Scientific Guardrails
1. **NO Synthetic Sine-Wave Claims:** Train/evaluate `WingBeat-TinyNet` strictly on real Oxford **HumBugDB** recordings under `Clean / 15 / 10 / 5 / 0 dB` SNR noise.
2. **NO 100% Species Separation Claim from f0 Alone:** Frame contribution honestly as **Pollinator-Safe Female Vector Guild Gating (`350–620 Hz` + `H2/H1`, `H3/H1` + Optical `dt_ms`)**.
3. **Sequential Fan-Muted Gating (MANDATORY):** Fan stays **100% OFF (`0%`)** during `512-pt` (`64.0 ms`) I2S FFT listening; `GPIO 13` triggers `5.0s` fan pulse ONLY on Class 2 Female Vector detection.
4. **Honest Heater & Scope Disclosure:** `100 Ohm` at `5V` = `0.25W` (`+2.5°C`); explicitly state acoustic playback of HumBugDB recordings if live caged mosquito trials are Phase-2.
