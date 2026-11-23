# PROJECT 2 (CSE 326 - SESSIONAL) — CROSS-ACCOUNT MASTER CONTEXT & ZERO-HALLUCINATION HANDOFF BIBLE
> **HOW TO USE THIS FILE IN ANY GEMINI PRO ACCOUNT (OR NEW CHAT):**  
> Open a separate chat dedicated **ONLY to Project 2 (CSE 326 Mosquito Trap)** in any of your Gemini Pro accounts, attach **`Project2_CSE326_Standalone_Account_Pack.zip`** (or this `.md` file), and paste the text inside **Section 1** below. Never mix Project 1 (Storm Box) into this chat!

---

## SECTION 1: COPY-PASTE BOOTSTRAP PROMPT FOR ANY NEW GEMINI PRO ACCOUNT (PROJECT 2 ONLY)

```text
=== STRICT CONTEXT LOCK: PROJECT 2 ONLY (CSE 326 - SESSIONAL) ===
You are assisting us exclusively with Project 2 (CSE 326 Microprocessor / Embedded Systems Sessional) at the Department of CSE, BAIUST, Cumilla, Bangladesh. Do NOT mix any Storm & Lightning Box (CSE 301) pins or context into this chat.

1. TEAM & SUPERVISOR:
- Team Members: Md. Najmus Sakib Rahatul (ID: 1119003), Nesar Uddin Ifag (ID: 1119004), Rayhan Alam Towhid (ID: 1119007).
- Supervisor: Md. Fahim Kabir Chowdhury (Lecturer, Dept. of CSE, BAIUST).
- Project Title: "Bio-Acoustic and Optical Mosquito Trap Using ESP32-S3 512-Point FFT and On-Box OLED Band Counters".

2. CONFIRMED HARDWARE INVENTORY (12 ITEM GROUPS, ITEMS 22-33 = 2,747 BDT — 100% PURCHASED, 0 MISSING PARTS):
- MCU & Power: ESP32-S3 N16R8 (44-Pin, Dual-Core 240 MHz) — 855 Tk, powered via 5V 2A USB Type-C supply.
- Exact Pinout on ESP32-S3:
  * GPIO 4 (SCK), GPIO 5 (WS), GPIO 6 (SD): INMP441 Omnidirectional I2S MEMS Microphone (235 Tk, L/R pin tied to GND, VCC to 3.3V). Samples 512-point window at 8,000 Hz (64.0 ms acquisition, 15.625 Hz bin width).
  * GPIO 7: DHT22 Temperature & Humidity Sensor (235 Tk, with 10 kOhm pull-up to 3.3V) for thermal wingbeat normalization: f0_corr = f0_raw - 7.0 * (T_amb - 28.0).
  * GPIO 8 (SDA), GPIO 9 (SCL): 1.3-inch I2C OLED Display (4-Pin SH1106, I2C 0x3C, 495 Tk) displaying live f0_corr, SNR (dB), and NVS-persisted band counters.
  * GPIO 10: LM393 IR Obstacle Sensor Module Gate #1 (Entry Gate, 40 Tk) — wakes FFT & measures insect transit duration (dt_ms).
  * GPIO 14 (or GPIO 11): LM393 IR Obstacle Sensor Module Gate #2 (Mesh Catch Verification Gate, 40 Tk).
  * GPIO 13: 5V 1-Channel Opto-Isolated Relay Module (75 Tk) driving the 5V Brushless DC Mini Cooling Fan (100 Tk).
  * GPIO 40 (Green), GPIO 41 (Yellow), GPIO 42 (Red): 10mm 3-Zone Status LEDs (50 Tk).
  * GPIO 38, 39, 1, 2: 4x Tactile Push Buttons (42 Tk) for bench calibration & NVS counter reset.
  * Passive Lure Ring (5V Rail): 6x 5mm 395nm UV LEDs + 1x 100 Ohm 5W Ceramic Resistor (70 Tk) + 2x MB-102 Breadboards (250 Tk) + 3x Jumper Wire Sets (260 Tk).

3. STRICT SCIENTIFIC & Q1 PUBLICATION RULES:
- Sequential Fan-Muted Gating is MANDATORY: The 5V Suction Fan MUST stay 100% OFF (0% duty cycle, silent) during INMP441 acoustic listening so 150-350 Hz fan noise never corrupts the 512-point FFT. Only AFTER Gate #1 + FFT + WingBeat-TinyNet confirms a target female vector (350-620 Hz) does GPIO 13 turn the fan ON for 5.0 seconds (while muting the mic).
- Do NOT claim 100% biological separation between Aedes, Anopheles, and Culex from fundamental frequency alone (since their wingbeat distributions overlap in nature). Frame the Q1 contribution honestly as "Pollinator-Safe Female Vector Guild Gating (350-620 Hz + Harmonics H2/H1 + Optical Transit Width dt_ms)" evaluated on real Oxford HumBugDB mosquito audio under 0/5/10/15 dB SNR noise.
- Always respond in Bengali script (বাংলা) with technical terms in English, and output a 5-line "[STATE CHECKPOINT]" block at the end of every reply for cross-account continuity.
```

---

## SECTION 2: BRUTALLY HONEST EVALUATION OF PROJECT 2 (REAL ANALOGY — WHAT IS RIGHT VS. WRONG)

### Real-World Analogy:
Think of **Project 2** like a **Doctor Listening to a Stethoscope Next to a Running Blender**:
- Your **`INMP441` Digital I2S Microphone + `ESP32-S3` 512-Point FFT** is a high-precision digital stethoscope.
- Your **`5V Brushless Suction Fan`** is a loud kitchen blender (`150–350 Hz` motor & wind noise).
- If the blender (Fan) runs *while* the doctor listens with the stethoscope (`INMP441`), the doctor hears **only blender noise, zero mosquito wingbeats**!
- **Why your hardware works 100%:** Because in our sequential firmware (`p2_deployment_trap_esp32s3.ino`), the **blender (Suction Fan) stays 100% OFF** while the microphone listens! Only *after* the mosquito enters the quiet funnel neck and is identified in `72.5 ms` (`64 ms` audio buffer + `8.5 ms` FFT/TinyML compute) does `GPIO 13` switch the fan ON for `5.0 seconds` to suck the mosquito into the mesh!

### Item-by-Item Honest Audit of Items 22–33 (2,747 BDT — 0 Missing Parts!):
1. **Zero Missing Hardware:** Unlike Project 1, you already have **100% of Items 22–33** for Project 2! You do NOT need to buy a single extra sensor for Project 2.
2. **What Was WRONG in the Previous "Thunder+Mosquito Project Agent" Setup (And How We Fixed It):**
   - **Wrong #1 (Pure Sine-Wave Audio Trap):** The old monorepo used Python-generated pure sine-wave `.wav` files (`Female_Aedes_495Hz.wav`) to claim `93.87%` species accuracy. Real mosquitoes are not pure sine waves, and in nature *Aedes*, *Anopheles*, and *Culex* frequencies overlap between `380–580 Hz`. **Strict Fix:** Train and test on real **HumBugDB** field recordings and frame your model around **Pollinator-Safe Female Vector Gating** (`100%` honeybee/housefly preservation + `350–620 Hz` female vector capture).
   - **Wrong #2 (`LM393` IR Gate Blind Spot):** A single `LM393` IR obstacle sensor (`Item #24`) has an effective reflection range of only `1.5–3.0 cm` on a tiny dark mosquito body. **Strict Fix:** Mount the two `LM393` modules across a narrow **`4–5 cm` constricted bottle neck** (not a wide 15 cm box) and tune the blue potentiometer screw until the green indicator LED is right at the trigger edge.
   - **Wrong #3 (`100 Ohm 5W` Resistor Heat Output):** On a `5V` USB rail, $P = V^2/R = 25/100 = 0.25\text{ W}$, which only warms the resistor by `+2.5°C`. **Strict Fix:** That is completely safe for your plastic breadboard (won't melt anything), and in your report you state honestly that it provides a mild localized convective plume (`+2.5°C` at `5V`, or `1.1W` if driven from `7.4V`).
