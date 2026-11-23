# WingBeat-TinyNet v6.0 Clean-Real: Transfer-Function-Invariant Bio-Acoustic and Optical Pollinator-Safe Mosquito Trap on ESP32-S3

**Complete Technical Reference & Project Book — CSE 326 (Microprocessor / Embedded Systems Sessional)**

| Field | Details |
| :--- | :--- |
| **Institution** | Department of Computer Science and Engineering (CSE), Bangladesh Army International University of Science and Technology (BAIUST), Cumilla Cantonment, Cumilla, Bangladesh |
| **Course** | CSE 326 — Microprocessor / Embedded Systems Sessional (Level-3 Term-2, 19th Batch, Section A, Group G1) |
| **Project Supervisor** | Md. Fahim Kabir Chowdhury, Lecturer, Department of CSE, BAIUST |
| **Team Members** | Md. Najmus Sakib Rahatul (ID: `1119003`), Nesar Uddin Ifag (ID: `1119004`), Rayhan Alam Towhid (ID: `1119007`) |
| **Target Platform** | `ESP32-S3 N16R8` (44-Pin, Dual-Core 240 MHz, 16 MB Flash, 8 MB OPI PSRAM) |
| **Firmware & Model Version** | `WingBeat-TinyNet v6.0 Clean-Real` — 100% Untouched Real Audio (`0%` synthetic `sin()`, `0%` artificial EQ/noise/pitch aug) + `130 Hz` Butterworth HPF + Local-Envelope Spectral Whitening `R[k]` + QAT 6-Class INT8 (`24->96->48->6`, `7.18 KB`) + Universal `DHT22` Thermal Bin Warping |
| **Verified Artifacts** | `wingbeat_q1_int8.h` (`SHA-256: C367F46DF8F904AD...`, `33,122 B`), `p2_deployment_trap_esp32s3.ino` (`SHA-256: 434DB5CC1790553F...`, `27,940 B`) |
| **Total Hardware Cost** | `2,757 BDT` (~$23 USD; 12 verified hardware groups + `10x 220 Ohm` current-limiting resistors) |

---

## Table of Contents

1. [Executive Summary & Problem Statement](#1-executive-summary--problem-statement)
2. [Literature Review & Five Research Gaps Addressed](#2-literature-review--five-research-gaps-addressed)
3. [Verified Hardware BOM & 44-Pin Wiring Architecture](#3-verified-hardware-bom--44-pin-wiring-architecture)
4. [Mechanical Enclosure Blueprint & Optical Gate Geometry](#4-mechanical-enclosure-blueprint--optical-gate-geometry)
5. [Embedded DSP, 130 Hz HPF & Local-Envelope Spectral Whitening](#5-embedded-dsp-130-hz-hpf--local-envelope-spectral-whitening)
6. [Universal DHT22 Thermal Spectral Bin Warping](#6-universal-dht22-thermal-spectral-bin-warping)
7. [24 Tilt-Invariant Features & WingBeat-TinyNet v6.0 QAT INT8 Architecture](#7-24-tilt-invariant-features--wingbeat-tinynet-v60-qat-int8-architecture)
8. [Leak-Free Clip-Disjoint Experimental Evaluation & Audits](#8-leak-free-clip-disjoint-experimental-evaluation--audits)
9. [Software Toolchain, Pre-Compiled Binaries & Portable Deployment](#9-software-toolchain-pre-compiled-binaries--portable-deployment)
10. [5-Minute Live Supervisor Demo & Real Lab Testing Playbook](#10-5-minute-live-supervisor-demo--real-lab-testing-playbook)
11. [Scientific Limitations, Honest Disclosures & Phase-2 Roadmap](#11-scientific-limitations-honest-disclosures--phase-2-roadmap)
12. [Teammate Division of Roles & 15 High-Impact Viva Q&A](#12-teammate-division-of-roles--15-high-impact-viva-qa)
13. [References](#13-references)

---

## 1. Executive Summary & Problem Statement

Mosquito-borne diseases — primarily **Dengue** and **Chikungunya** (*Aedes aegypti*, *Aedes albopictus*), **Malaria** (*Anopheles* spp.), and **Lymphatic Filariasis** (*Culex quinquefasciatus*) — impose a recurring public health burden across Bangladesh and tropical South Asia. Conventional electric UV suction traps suffer from four fundamental engineering flaws:

1. **Acoustic Self-Masking:** Commercial suction traps run their electric fan continuously (`100%` duty cycle), generating `150–350 Hz` motor harmonics and turbulent airflow noise that completely bury faint mosquito wingbeat acoustics (`~30–40 dB SPL`).
2. **Ecological Collateral Damage:** UV-only traps indiscriminately kill beneficial pollinators (honeybees, bumblebees, solitary bees) and non-biting male mosquitoes alongside biting female vectors.
3. **Lab-Rumble Shortcut Learning & Device-EQ Sensitivity:** Acoustic ML models trained on raw laboratory recordings frequently memorize `50–125 Hz` chamber AC rumble ("Clever Hans" shortcut) and global spectral tilt, causing them to fail when deployed in quiet outdoor rooms or tested across different microphone/speaker hardware.
4. **Zero Surveillance Telemetry & Thermal Drift:** Standard traps provide no genus-level epidemiological count and ignore the ectothermic biology of mosquitoes, whose wingbeat fundamental frequency (`f0`) shifts by approximately `+7.0 Hz` per `+1°C` of ambient temperature.

### Core Intuitive Analogy (For Supervisor & Defense Presentation)
> **The Doctor, the Stethoscope, and the Kitchen Blender:**  
> Think of the `INMP441` I2S MEMS microphone and `ESP32-S3` 512-point FFT as a doctor listening with a digital stethoscope, and the `5V` suction fan as a noisy kitchen blender. If the blender runs while the doctor listens, the doctor hears only blender noise and zero mosquito wingbeats. Our **Sequential Fan-Muted Gating** architecture keeps the fan **100% OFF (`0%` duty cycle)** during every `64.0 ms` acoustic listening window. Only after four pitch-locked frames (`|Δf0| <= 38 Hz`, `|f0 - anchor| <= 46 Hz`) verify a biting female disease vector (`340–660 Hz`) with whitened 2nd and 3rd harmonics (`RH2 >= 2.2`, `RH3 >= 2.0`) does `GPIO 13` trigger a `5.0 s` suction capture pulse, followed by a `1.5 s` acoustic settling cooldown and I2S DMA buffer flush.

### Key Verified Achievements (`WingBeat-TinyNet v6.0 Clean-Real` — 0% Synthetic / Leak-Free GroupCV)
- **100.0% Untouched Real Dataset (`n = 13,256` Frames, `1,580` Unique Clips/Blocks):** Built strictly from 5 real acoustic sources (*PLOS ONE MosquitoSong+*, *Oxford HumBugDB*, *ESC-50*, *Wikimedia Commons Real Bees*, and *Cumilla Field Room/Vocal Recordings*) with **0 synthetic `np.sin()` waveforms and 0 artificial EQ/AWGN/pitch augmentations**.
- **Strict Clip-Disjoint 5-Fold GroupCV (Zero Frame-Overlap Leakage):**
  - **Tier-1 Female Vector Guild Accuracy:** **`94.42%`** (`INT8`) / `94.43%` (`FP32`); Unseen Clip Holdout (`n = 2,658`): **`93.98%`**.
  - **Pollinator (Bee) Preservation Rate:** **`100.00%`** (`0 / 1,613` real outdoor bee frames misclassified as vector; reinforced by Stage-2 optical transit-width veto `26–60 ms`).
  - **Tier-2 6-Class Multi-Genus Attribution:** **`87.08%`** (`Macro-F1: 83.22%`, Noise Rejection **`97.03%`**) in Straight-Through QAT `INT8` inside a **`7.18 KB`** weight tensor (virtually zero quantization drop vs. `FP32` `87.09%` / `83.21%`).
- **Transfer-Function-Invariant DSP (`130 Hz` HPF + Local-Envelope Spectral Whitening `R[k]`):** Strips `50–125 Hz` Thailand lab AC rumble & Bangladesh ceiling-fan motor rumble (cutting live room `RMS` by `>2×`, enabling adaptive `MIN_ACTIVE_RMS = 160` for faint live mosquitoes at `2–6 cm`), while keeping `22 / 24` features invariant (`<= 0.03` diff) between a flat live mic and a `720 Hz` HPF phone speaker.
- **Offline Real-Recording Firmware Audit (`64 ms` Cadence, `streak = 4`):** `0` false fan triggers on Sakib's real `"mew-mew"` vocal hum (`step=512`), room speech/sawmill, real Wikimedia bees (`44.1 s` + `20.1 s`), and males; **`135 / 136` (`99.26%`)** ESC-50 WAVs zero false triggers (1 emergency siren clip `1-54084-A-42.wav` honestly reported); **`20 / 20`** female vector library WAVs triggered and **`23 / 23`** non-vector library WAVs zero false triggers across quiet live-air (`RMS = 260`) and generic small phone speakers.
- **Standalone Edge Deployment (`COM5` Live-Flashed & Verified):** **`2,757 BDT`** electronics (`+~340 BDT` DIY 4" PVC enclosure & `70×90 mm` perfboard), `356,573 B` (`11%`) Flash, `35,144 B` (`10%`) RAM; live `[P2v6.0]` telemetry verified (`T: 24.1–24.2°C`, `IR:OK/OK`, post-HPF room `RMS 423–685`, adaptive `FLR 391–588`, `0` room false triggers).

---

## 2. Literature Review & Five Research Gaps Addressed

To position this work rigorously for both CSE 326 evaluation and Q1 manuscript submission, we benchmarked our architecture directly against six peer-reviewed foundational publications:

1. **Villarreal et al. (2017)**, *Journal of Medical Entomology*: Proved experimentally that female *Aedes aegypti* wingbeat frequency increases linearly by `~8–13 Hz/°C` with ambient temperature regardless of body size or mating state.
2. **Mukundarajan et al. (2017)**, *eLife* (*Abuzz*): Demonstrated smartphone-based acoustic mosquito surveillance across 20 species, but relied on manual human phone placement, cloud/offline processing, and performed no selective physical actuation or pollinator protection.
3. **Potamitis et al. (2017–2020)**, *Sensors (MDPI)*: Pioneered optoacoustic insect e-traps using optical photodiode arrays, yet commercial/research optical arrays cost significantly more than MEMS audio + dual `LM393` gates and typically run continuous fans.
4. **Kiskin et al. (2021)**, *NeurIPS Datasets & Benchmarks* (*HumBugDB*): Released the large-scale Oxford free-flight mosquito dataset and warned against recording-environment shortcut learning while evaluating desktop-scale Bayesian CNNs (`MozzBNN`) unsuitable for sub-10 KB MCU SRAM budgets.
5. **Supratak et al. (2024)**, *PLOS ONE* (*MosquitoSong+*): Curated 4-species *Ae. aegypti*, *Ae. albopictus*, *An. dirus*, and *Cx. quinquefasciatus* recordings (`8,000 Hz`, `25°C` lab chamber) alongside outdoor distractors, confirming that continuous CDC fan noise degrades classification.
6. **Piczak (2015)**, *ACM Multimedia* (*ESC-50*): Established the 50-class environmental sound benchmark used to stress-test acoustic edge detectors against real-world false positives.

### Gap-Mapping Matrix: Prior Literature vs. WingBeat-TinyNet v6.0 Clean-Real

| Prior Work | Primary Strength | Unresolved Engineering / Field Gap | How `WingBeat-TinyNet v6.0 Clean-Real` Solves the Gap |
| :--- | :--- | :--- | :--- |
| **Villarreal et al. (2017)** | Quantified ectothermic `+7–13 Hz/°C` wingbeat shift | No embedded real-time spectral warping algorithm | **Universal `DHT22` Thermal Spectral Bin Warping** shifts `f0`, centroid, and all `6` biological FFT subbands automatically (`10–45°C` clamp) |
| **Mukundarajan / Abuzz (2017)** | Multi-species smartphone acoustic ID | Manual holding, no trap actuation, no pollinator veto | Autonomous `ESP32-S3` trap with **Sequential Fan-Muted Gating** + **100% Bee Safe** optical & neural veto |
| **Kiskin / HumBugDB (2021)** | Free-flight benchmark & shortcut-learning warning | Heavy GPU/CPU CNNs (`MB`-scale); lab-rumble domain shift | **`130 Hz` HPF + Local-Envelope Spectral Whitening `R[k]`** + **`7.18 KB` QAT INT8 MLP** (`11%` Flash, `10%` RAM) validated via Clip-Disjoint GroupCV |
| **Supratak / MosquitoSong+ (2024)** | 4-species + outdoor noise recordings at `25°C` | Fixed `25°C` lab chamber; `50–125 Hz` AC rumble; continuous fan | **Fan `0%` OFF during listening** + `130 Hz` HPF rumble removal + live `DHT22` normalization + `135/136` ESC-50 rejection |
| **Potamitis et al. (2017–2020)** | Optical wingbeat sensing e-traps | Higher optical-array BOM cost; limited on-box genus telemetry | **`2,757 BDT` ($23) hybrid trap**: `INMP441` audio + dual `LM393` IR gates (`dt_ms` veto + catch verify) + NVS OLED counters |

---

## 3. Verified Hardware BOM & 44-Pin Wiring Architecture

### 3.1 Verified Bill of Materials (100% Physically Owned)

| # | Component / Module | Model / Spec | Qty | Role in System | Cost (BDT) |
| :-: | :--- | :--- | :-: | :--- | :-: |
| 1 | Microcontroller | `ESP32-S3 N16R8` (44-Pin, Dual USB-C) | 1 | 240 MHz FFT DSP, INT8 TinyML, OLED/Relay/NVS control | 855 |
| 2 | I2S MEMS Microphone | `INMP441` Omnidirectional Digital I2S | 1 | `8,000 Hz`, 512-sample wingbeat acoustic capture (`3.3V` ONLY) | 235 |
| 3 | Temp & Humidity Sensor | `DHT22` (`V182` 3-pin PCB, built-in pull-up) | 1 | Live ambient `T_amb` for Universal Thermal Spectral Bin Warping | 235 |
| 4 | On-Box OLED Display | `1.3" SH1106` I2C OLED (`DST-013`, `0x3C`) | 1 | Real-time status, `AED/ANO/CUL`, `Cat/Bee/Mal`, `T/H` diagnostics | 495 |
| 5 | Optical IR Gates | `HW-201 / FC-51` (`LM393` Comparator) | 2 | Gate #1: Entry `dt_ms` Bee Veto; Gate #2: Mesh Catch Verification | 80 |
| 6 | Suction Relay | `5V 1-Channel Relay` (`JQC3F-05VDC-C`, Active-LOW) | 1 | Opto-isolated switching of `5V` suction fan (`NO` contact) | 75 |
| 7 | Suction Fan | `5V Brushless DC Mini Fan` (`JD-4007S5L2`) | 1 | `5.0 s` selective vector suction pulse (100% OFF while listening) | 100 |
| 8 | 3-Zone Status LEDs | `10mm` LEDs (Green, Yellow, Red) | 3 | Visual state: `BEE SAFE` (G), `MALE IGN` (Y), `VECTOR/FAN` (R) | 50 |
| 9 | Control Buttons | Yellow Tactile Push Buttons (4-pin) | 4 | Fan/LED test, OLED Page toggle, Serial toggle, NVS Counter Reset | 42 |
| 10 | Passive Lure Ring | `6x 5mm 395nm UV LEDs` + `1x 100 Ohm 5W` Ceramic | 7 | Mild optical UV attraction + `+2.5°C` (`0.25W`) convective cue | 70 |
| 11 | Breadboards | `MB-102` 830-Point Solderless Breadboards | 2 | Dual-rail prototyping (NOTE: bridge Row 30 split!) | 250 |
| 12 | Jumper Wires & Resistors | 3x Jumper Sets + `10x 220 Ohm (1/4W)` Resistors | — | Signal routing & mandatory LED current limiting (~`10 BDT`) | 270 |
| | **TOTAL** | **Complete Standalone Hardware System** | | **Power Source:** `5V 20W` Power Bank or PC USB-C (`UART` port) | **2,757 BDT** |

### 3.2 Dual Power-Rail Isolation & Exact 44-Pin ESP32-S3 Pinout

> **CRITICAL SAFETY RULES:**
> 1. **Never power `INMP441`, `DHT22`, `SH1106 OLED`, or `HW-201 IR` from `5Vin`!** Keep all sensors strictly on the quiet **`3V3` rail** so relay switching spikes never reset the I2S bus or damage the `3.6V`-max `INMP441`.
> 2. **Never wire `6x 395nm UV LEDs` to any GPIO pin!** Connect all 6 in parallel (`~49 mA` total) plus the `100 Ohm 5W` resistor (`50 mA`) directly to the **`5Vin` actuator rail** via individual `220 Ohm` resistors.
> 3. **MB-102 Breadboard Row-30 Split:** Bridge left-to-right rail halves across Row 30/31 and tie all `GND` rails together (**Common Ground**).
> 4. **ESP32-S3 Dual USB-C Identification:** Holding the board upright with the metal RF shield at the top, the **Right USB-C port is `UART` (`CH343`)** (`CDCOnBoot=default`), and the **Left USB-C port is Native `USB` OTG** (`CDCOnBoot=cdc`).

| ESP32-S3 Pin | Connected Module & Pin | Power Rail | Electrical / Firmware Notes |
| :--- | :--- | :---: | :--- |
| **`GPIO 4`** | `INMP441` — `SCK` (Serial Clock) | `3V3` | I2S bit clock (`8,000 Hz` sample rate) |
| **`GPIO 5`** | `INMP441` — `WS` (Word Select) | `3V3` | I2S left/right frame clock |
| **`GPIO 6`** | `INMP441` — `SD` (Serial Data) | `3V3` | 32-bit I2S PCM stream (`L/R` pin tied to `GND`!) |
| **`GPIO 7`** | `DHT22` (`V182`) — `out` | `3V3` | Built-in PCB pull-up; polled every `2.5 s`, clamped `10–45°C` |
| **`GPIO 8`** | `SH1106 OLED` (`DST-013`) — `SDA` | `3V3` | Hardware I2C `0x3C`; OLED pin order: `GND \| VCC \| SCL \| SDA` |
| **`GPIO 9`** | `SH1106 OLED` (`DST-013`) — `SCL` | `3V3` | Auto-recovers on hot-plug via `u8g2.begin()` |
| **`GPIO 10`** | `HW-201 IR Gate #1` — `OUT` | `3V3` | Entry transit width `dt_ms` (`26–60 ms` = Optical Bee Veto) |
| **`GPIO 14`** | `HW-201 IR Gate #2` — `OUT` | `3V3` | Mesh catch verification during `5.0 s` fan pulse |
| **`GPIO 13`** | `5V Relay` (`JQC3F-05VDC-C`) — `IN` | `5Vin` | **Active-LOW** (`LOW` = Fan ON); Load: `COM=5Vin`, `NO=Fan(+)`, `Fan(-)=GND` |
| **`GPIO 40`** | `10mm Green LED` (`+220 Ohm` series) | GPIO `3.3V` | Long leg (`+`) via `220 Ohm` to `GPIO 40`; Short leg (`-`) to `GND` -> `BEE SAFE` |
| **`GPIO 41`** | `10mm Yellow LED` (`+220 Ohm` series) | GPIO `3.3V` | Long leg (`+`) via `220 Ohm` to `GPIO 41`; Short leg (`-`) to `GND` -> `MALE IGN` |
| **`GPIO 42`** | `10mm Red LED` (`+220 Ohm` series) | GPIO `3.3V` | Long leg (`+`) via `220 Ohm` to `GPIO 42`; Short leg (`-`) to `GND` -> `VECTOR / FAN` |
| **`GPIO 38`** | Yellow Button `BTN1` (to `GND`) | `INPUT_PULLUP` | Press to run `1.5 s` Relay Fan + 3-LED self-test |
| **`GPIO 39`** | Yellow Button `BTN2` (to `GND`) | `INPUT_PULLUP` | Press to toggle OLED `Main` <-> `Diagnostics` page |
| **`GPIO 1`** | Yellow Button `BTN3` (to `GND`) | `INPUT_PULLUP` | Press to toggle verbose `115200` baud Serial telemetry |
| **`GPIO 2`** | Yellow Button `BTN4` (to `GND`) | `INPUT_PULLUP` | Press to reset all NVS Flash counters to zero |
| **`5Vin` / `GND`** | `6x 395nm UV LEDs` (`6x 220 Ohm` each) | `5Vin` | Each UV LED: `5Vin -> 220 Ohm -> Anode (+) -> Cathode (-) -> GND` (`~8.2 mA` each) |
| **`5Vin` / `GND`** | `100 Ohm 5W` Ceramic Resistor | `5Vin` | Directly across `5Vin` & `GND`: `I = 50 mA`, `P = 0.25 W`, `ΔT ≈ +2.5°C` (breadboard-safe) |

### 3.3 Resistor & Thermal Calculations (Viva-Ready Equations)
- **Status LED Current (`3.3V` GPIO, `220 Ohm`):** Assuming `V_f ≈ 2.0 V` (Red/Yellow) to `2.2 V` (Green), $I_{\text{LED}} = (3.3 - 2.0)/220 \approx 5.9\text{ mA}$ — well below the `ESP32-S3` `20 mA` safe pin limit.
- **395 nm UV LED Current (`5.0V` rail, `220 Ohm` each):** With `V_f ≈ 3.2 V`, $I_{\text{UV}} = (5.0 - 3.2)/220 \approx 8.2\text{ mA}$ per LED (`~49 mA` total across 6 LEDs).
- **Ceramic Resistor Thermal Plume (`100 Ohm 5W` on `5V`):** $P = V^2 / R = 5^2 / 100 = 0.25\text{ W}$ (`I = 50 mA`), raising surface temperature by `~+2.5°C` above ambient (safe for ABS breadboards; honest mild thermal cue).

---

## 4. Mechanical Enclosure Blueprint & Optical Gate Geometry

Inexpensive `HW-201 / FC-51` (`LM393`) infrared obstacle modules have short reflective detection range (`1.5–3.0 cm`) on small insects. A constricted `4–5 cm` (2" PVC) throat eliminates the central IR blind spot and concentrates acoustic energy onto `INMP441`.

```
         [ 6x 395nm UV LED Ring + 100 Ohm Thermal Cue ]
              \                                  /
               \      Top Funnel Intake         /
                \______________________________/
                         |    4–5 cm    |   <-- Constricted Neck (Eliminates IR Blind Spot)
    [INMP441 Mic] -----> |  Listening   | <---- [HW-201 Gate #1: Entry dt_ms Bee Veto]
    (Foam isolated)      |   Chamber    |       [DHT22 Sensor: Ambient T_amb / RH%]
                         |______________|
                         |  5V Suction  |   <-- 100% OFF during listening; 5.0s ON on Vector
                         |     Fan      |
                         |______________|
                         |  Fine Mesh   | <---- [HW-201 Gate #2: Mesh Catch Verification]
                         | Capture Cup  |
                         +--------------+
```

- **Software Stuck-Glare Immunity (`IR_STUCK_MS = 150 ms`):** If direct sunlight or an uncalibrated potentiometer holds `PIN_GATE_1` or `PIN_GATE_2` continuously `LOW` for `> 150 ms`, the firmware flags `IR:POT` and ignores stuck glare rather than falsely counting `Cat` or blocking vector detection.

---

## 5. Embedded DSP, 130 Hz HPF & Local-Envelope Spectral Whitening

Every loop iteration executes a deterministic, non-blocking 5-stage pipeline on the `ESP32-S3` that matches Python (`p2_humbugdb_q1_training_and_snr_eval.py`) bit-for-bit:

1. **Stage 1 — Silent I2S Acquisition (`64.0 ms`) & `130 Hz` Butterworth HPF:** The fan relay (`GPIO 13`) is strictly OFF (`HIGH`, Active-LOW). `i2s_read()` captures `512` 32-bit samples at `8,000 Hz` (`15.625 Hz/bin`). After DC removal, a causal 2nd-order Butterworth High-Pass Filter (`fc = 130 Hz`, `b = [0.9303447533, -1.8606895066, 0.9303447533]`, `a = [1.0, -1.8558317538, 0.8655472595]`) removes `50/60/120 Hz` mains hum, ceiling-fan motor rumble, and lab-chamber AC rumble without touching any bee (`140–290 Hz`) or mosquito (`340–1020 Hz`) fundamental. Post-HPF RMS (`e_rms`) is compared against an adaptive quiet-room floor: `dyn_rms_floor = max(160.0, min(2000.0, 1.18 * ambient_rms_ema))`, allowing quiet live mosquitoes (`RMS 180–450` at `2–6 cm`) to enter the network while automatically adapting upward in noisy rooms.
2. **Stage 2 — Local-Envelope Spectral Whitening (`R[k]`):** After Hann windowing and 512-point real FFT (`spec[0..255]`), each bin is normalized by its outer flanking envelope (`k-8..k-3` and `k+3..k+8`):

$$R[k] = \frac{X[k]}{\frac{1}{|\mathcal{N}(k)|}\sum_{j \in \mathcal{N}(k)} X[j] + 10^{-6}}, \qquad E[k] = \max(0,\; R[k] - 1.35)$$

   Because any smooth acoustic transfer function $G(f)$ (microphone, plastic funnel resonance, Android/iPhone/laptop speaker roll-off) varies slowly across `±125 Hz`, $G(f_k)$ cancels cleanly from both numerator and denominator (`R_obs[k] ≈ R_true[k]`). Empirically, `22 / 24` extracted features agree to `<= 0.03` between a flat microphone and a steep `720 Hz` HPF phone speaker — achieved purely via signal processing with **zero synthetic EQ augmentation**.
3. **Stage 3 — Unified `0.5× / 1.5×` Sub-Octave Fundamental Recovery:** In whitened space `R[k]`, if the dominant peak sits in `297–1218 Hz` (`bins 19..78`) and BOTH `0.5×peak` (`r_half >= 2.3`) AND `1.5×peak` (`r_3h >= 2.2`) are sharp tonal peaks, `peak_bin` snaps down to `0.5×peak`. This single physical rule simultaneously snaps `2×f0`-dominant real outdoor bees (`340–570 Hz`) back to `140–285 Hz` (`BEE SAFE`) AND small-speaker `2×f0`-peaking female mosquitoes (`680–1218 Hz`) back to `340–650 Hz`.
4. **Stage 4 — Hierarchical 6-Class QAT INT8 Inference + Whitened Harmonic Guards:** `runWingBeatQ1Probs()` evaluates the `24->96->48->6` (`7.18 KB`) Straight-Through QAT INT8 MLP (`p_vec = P[2] + P[3] + P[4]`):
   - **Female Vector Guild (`tier1 = 2`):** `p_vec >= 0.72`, `340 <= f0_corr <= 660 Hz`, `RH2 >= 2.2`, `RH3 >= 2.0`, `sub_pk_w <= 0.58`.
   - **Male Mosquito (`tier1 = 5`):** `P[5] >= 0.72`, `665 <= f0_corr <= 1020 Hz`, `RH2 >= 2.0`, `RH3 >= 1.8`, `sub_pk_w <= 0.58`.
   - **Pollinator / Bee (`tier1 = 1`):** `P[1] >= 0.70`, `135 <= f0_corr <= 290 Hz`, `RH2 >= 1.7` (plus Stage-2 Optical Bee Veto `dt_ms` `26–60 ms` on healthy `Gate #1`).
5. **Stage 5 — 4-Frame Pitch-Locked Anchor Vote & Selective `5.0 s` Capture:** Requires `4` consecutive `tier1 = 2` frames (`~256 ms`) with `|f0(t) - f0(t-1)| <= 38 Hz` and `|f0(t) - anchor_f0| <= 46 Hz` to energize the `5V` fan relay for `5.0 s`, verify mesh catch on `Gate #2`, update NVS Flash counters, enforce `1.5 s` cooldown, and flush the I2S DMA buffer.

---

## 6. Universal DHT22 Thermal Spectral Bin Warping

Mosquitoes are ectothermic insects whose wingbeat fundamental shifts by `~+7.0 Hz/°C` (Villarreal et al., 2017). Because `DHT22` (`GPIO 7`) reads live ambient `T_amb` every `2.5 s` (`-10..+50°C` live display, clamped to `[10.0°C, 45.0°C]` for biological flight-muscle physics), both Python training and ESP32-S3 firmware warp every spectral band boundary onto the `28.0°C` canonical reference grid without manual temperature entry:

$$\Delta f_T = 7.0 \times (T_{\text{eff}} - 28.0), \qquad b_{\text{sh}} = \operatorname{round}\!\left(\frac{\Delta f_T}{15.625}\right), \qquad f_{0,\text{corr}} = f_{0,\text{raw}} - \Delta f_T$$

Every frequency index (`k_lo, k_hi, k9, k14, k21, k28, k35, k42, k68`) shifts by `b_sh` (clamped safely to `[9, 175]`), keeping *Culex*, *Aedes*, *Anopheles*, *Bees*, and *Males* aligned from cool winter evenings (`14–18°C`) to peak Bangladesh summer heat (`34–42°C`).

---

## 7. 24 Tilt-Invariant Features & WingBeat-TinyNet v6.0 QAT INT8 Architecture

### 7.1 Complete 24 Transfer-Function-Invariant Feature Vector (`P2_N_FEATURES = 24`)

| Group | # | Feature Name | Physical Meaning |
| :--- | :-: | :--- | :--- |
| **Fundamental & Local SNR** | 1–2 | `f0_corr`, `snr_db` | Thermally corrected fundamental (`Hz`) & whitened local peak prominence `20*log10(max(pk_R, 1))` (`dB`) |
| **Whitened Harmonic Comb** | 3–5 | `h2_prom`, `h3_prom`, `h4_prom` | Log local prominence `log1p(R[m*f0])` at 2nd, 3rd, and 4th harmonics (100% speaker-EQ-invariant) |
| | 6–9 | `h2_h1_w`, `h3_h1_w`, `h3_h2_w`, `harm_comb_w` | Whitened harmonic-to-fundamental ratios and 3-harmonic comb strength |
| **Sub-Harmonic & Peak Shape** | 10–12 | `sub_pk_w`, `pk3_tonal_w`, `skirt_w` | Ceiling-fan-safe whitened sub-harmonic ratio (`b3 >= 12`), 3-bin excess-tonal concentration, peak skirt sharpness |
| **Whitened Spectral Shape** | 13–16 | `spec_flatness_w`, `spec_entropy_w`, `w_centroid`, `w_spread` | Flatness, normalized entropy, thermally warped centroid, and spread computed strictly on whitened `R[k]`/`E[k]` |
| **6 Warped Bio Sub-Bands** | 17–22 | `wb_140_220`, `wb_220_328`, `wb_328_438`, `wb_438_547`, `wb_547_656`, `wb_656_1062` | Relative whitened excess energy `log1p(100 * Ep_sub / Ep_bio)` cleanly partitioning Bees, *Culex*, *Aedes*, *Anopheles*, and Males |
| **Upper Harmonic Density** | 23–24 | `hi_crest_w`, `hi_tonal_cnt` | Gain-invariant crest `log1p(max(R[1062..2800]))` and fraction of sharp tonal bins (`R >= 2.5`) above `1.06 kHz` |

### 7.2 Neural Network Topology & Verified `v6.0` ESP32-S3 Footprint
- **Architecture:** `24 -> 96 (ReLU) -> 48 (ReLU) -> 6 (Softmax)` (`7,350` params: `7,200` INT8 weights = `7.18 KB` + `150` FP32 biases).
- **Training Protocol (`v6.0 Clean-Real`):** Adam + Cosine Annealing (`620` epochs, label smoothing `0.025`, mid-training hard-negative mining `1.40×`, final `120`-epoch Straight-Through QAT) with 6-round champion selection on strict clip-disjoint holdout (`seed=106, lr=0.0095, l2=1.8e-4`).
- **Symmetric Per-Layer INT8 Export (`wingbeat_q1_int8.h`):**
  $$s_l = \frac{\max(|W_l|) + 10^{-8}}{127}, \qquad W_{l,\text{int8}} = \operatorname{clip}\!\left(\operatorname{round}\!\left(\frac{W_l}{s_l}\right), -127, 127\right)$$
- **Verified `ESP32-S3` Footprint (`v6.0 Clean-Real`):**
  - `UART` Binary (`CDCOnBoot=default`): **`356,573 B`** sketch / **`356,944 B`** `.bin` (**`11%`** Flash) | Global RAM: **`35,144 B` (`10%`)**.
  - `USB` Binary (`CDCOnBoot=cdc`): **`345,073 B`** sketch / **`345,440 B`** `.bin` (**`10%`** Flash) | Global RAM: **`34,936 B` (`10%`)**.

---

## 8. Leak-Free Clip-Disjoint Experimental Evaluation & Audits

### 8.1 Five Untouched Real Datasets (`n = 13,256` Frames — `0` Synthetic, `0` Artificial Aug)
1. **PLOS ONE 2024 *MosquitoSong+* (Supratak et al., 2024):** Untouched real recordings (`25.0°C` lab) of female *Ae. aegypti* (`1,300`), *Ae. albopictus* (`550`), *An. dirus* (`650`), *Cx. quinquefasciatus* (`1,200`), males (`750`), and 12 real outdoor environmental distractors (`650`).
2. **Oxford *HumBugDB* (Kiskin et al., 2021):** Untouched `R4_cleaned` free-flight *Culex* (`550`) & non-mosquito (`350`), CDC female/male species (`859`), and urban `background_with_speech`, `bus_stop`, `construction_site` (`800`).
3. **ESC-50 Environmental Noise Benchmark (Piczak, 2015):** `136` real WAVs across `17` categories (`1,900` non-overlapping `step=512` frames).
4. **Wikimedia Commons Real Outdoor Bees (CC-BY/CC0):** `1,613` untouched real frames from `Hummel_bee.ogg` (`550`), `Bee_buzzing_sound` (`550`), and `Buzzing_bees.ogg` (`513`) — **zero `np.sin()` waveforms**.
5. **Live Cumilla Field Room & Vocal Recordings (`28.7°C`):** Real room ceiling fan + distant sawmill (`700`), normal speech (`800`), and close-mic `"mew-mew"` vocal hum (`600`).

### 8.2 Table 1 — Leak-Free Clip-Disjoint 5-Fold GroupCV Ablation (`v6.0 Clean-Real`, `n = 13,256`)

Every recording / 5-second block (`clip_id`, `1,580` unique groups) is strictly isolated inside a single fold — **zero overlapping frames ever cross between training and validation**:

| Model / Baseline | 6-Class Acc | 6-Class Macro-F1 | Tier-1 Guild Acc | Pollinator (Bee) Safe | Noise Rejection |
| :--- | :---: | :---: | :---: | :---: | :---: |
| A. Bandpass Frequency Rule Only | `59.12%` | `58.73%` | `72.21%` | `100.00%` | `77.71%` |
| B. Linear Logistic Regression (24-feat) | `79.47%` | `74.77%` | `88.22%` | `99.69%` | `95.74%` |
| C. `WingBeat-TinyNet v6.0` (`FP32`, GroupCV) | `87.09%` | `83.21%` | `94.43%` | **`100.00%`** | `97.09%` |
| **D. `WingBeat-TinyNet v6.0` (`QAT INT8`, GroupCV)** | **`87.08%`** | **`83.22%`** | **`94.42%`** | **`100.00%`** | **`97.03%`** |
| **E. 6-Round QAT Champion (`seed=106`, Unseen Clip Holdout `n=2,658`)** | **`87.17%`** | **`83.46%`** | **`93.98%`** | **`100.00%`** | **`96.55%`** |

### 8.3 Table 2 — Full 6×6 Confusion Matrix (`WingBeat-TinyNet v6.0 Clean-Real INT8`, Clip-Disjoint 5-Fold GroupCV, `n = 13,256`)

| True \ Predicted | `C0 Noise` | `C1 Bee` | `C2 Aedes` | `C3 Anoph.` | `C4 Culex` | `C5 Male` | Total (`n`) | Precision | Recall | F1-Score |
| :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| **`C0` Noise/Env** | **5,382** | 149 | 72 | 21 | 79 | 97 | 5,800 | `94.44%` | `92.79%` | `93.61%` |
| **`C1` Pollinator** | 107 | **1,506** | **0** | **0** | **0** | 0 | 1,613 | `90.94%` | `93.37%` | `92.14%` |
| **`C2` Aedes** | 85 | 0 | **1,642** | 251 | 232 | 0 | 2,210 | `78.15%` | `74.30%` | `76.18%` |
| **`C3` Anopheles** | 8 | 0 | 146 | **459** | 36 | 1 | 650 | `57.38%` | `70.62%` | `63.31%` |
| **`C4` Culex** | 81 | 1 | 239 | 69 | **1,483** | 0 | 1,873 | `80.99%` | `79.18%` | `80.08%` |
| **`C5` Male** | 36 | 0 | 2 | 0 | 1 | **1,071** | 1,110 | `91.62%` | `96.49%` | `93.99%` |

> **Honest Biological Interpretation:** **`0 / 1,613`** real outdoor bees ever leak into any mosquito class (`100.00%` Bee Safe), while **`96.15%` of *Aedes*, `98.62%` of *Anopheles*, and `95.62%` of *Culex*** stay strictly inside the biting Female Vector Guild `{C2, C3, C4}`. Inter-genus overlap (`~15–25%` between *Aedes*/*Anopheles*/*Culex*) reflects true biological `f0` overlap across unseen recordings, honestly justifying our Two-Tier Guild + 4-Frame Temporal Voting design.

### 8.4 Table 3 — Device-Neutral Offline Real-Recording Audit (Exact Exported `v6.0` INT8 C-Header, `streak = 4`)

| Real Test Recording / Benchmark | Quiet Live-Air (`RMS=260`) | Android Spk (`550 Hz` HPF) | Small Phone (`720 Hz` HPF) | Tiny Spk (`950 Hz` HPF) | Result |
| :--- | :---: | :---: | :---: | :---: | :--- |
| `01_Aedes_aegypti` (`5.5 s` uncut) | **`20`** (`20 AED`) | **`21`** (`21 AED`) | **`21`** (`21 AED`) | **`21`** (`21 AED`) | Consistent across all 4 channels |
| `02_Anopheles_dirus` (`14.0 s` uncut) | **`13`** (`7 AED, 6 ANO`) | **`16`** (`10 ANO, 4 AED`) | **`16`** (`11 ANO, 3 AED`) | **`15`** (`11 ANO, 3 AED`) | Consistent across all 4 channels |
| `03_Culex` (*HumBugDB R4*, `12.0 s`) | **`20`** (`11 AED, 9 CUL`) | **`28`** (`24 CUL`) | **`26`** (`22 CUL`) | **`27`** (`24 CUL`) | Consistent across all 4 channels |
| `04_Real_Wikimedia_Bee` + `Hummel` (`44s`) + `Buzz` (`20s`) | **`0` Fan** (`555` Bee frames) | — | — | — | **100% Bee Safe** |
| `05_Real_Male_Mosquito` (`7.3 s`) | **`0` Fan** (`49` Male frames) | — | — | — | **100% Male Ignored** |
| Sakib `"mew-mew"` (`37.5 s`, `step=512`), `VID1_talk`, `Room_sawmill` | **`0` Fan** | — | — | — | **0 Vocal/Room False Triggers** |
| **ESC-50 ALL `136` WAVs (17 Cats)** | **`135 / 136` (`99.26%`)** | — | — | — | Honest: 1 siren clip `1-54084-A-42.wav` |
| **Full `43`-Track Web Library (`web_demo/`)** | **`20/20` Vec \| `23/23` Safe** | — | — | — | Verified with `0` synthetic files |

### 8.5 Live Hardware Serial Verification (`COM5`, `ESP32-S3 N16R8`)
After flashing `v6.0 Clean-Real` (`UART` `356,944 B`, `Hash of data verified.`), live `8 s` `[P2v6.0]` telemetry confirmed `T: 24.1–24.2°C`, `IR:OK/OK`, post-HPF room `RMS 423–685` (cut `>2×` vs. pre-HPF `1,100–1,450`), adaptive `FLR 391–588`, and `T1:0` (`P = 0.90–1.00`, `STK:0`, `0` false triggers) against room noise. Physical bench trials with live captured mosquitoes and multi-phone speakers are performed directly on the flashed board in the lab.

---

## 9. Software Toolchain, Pre-Compiled Binaries & Portable Deployment

- **Arduino Core:** `esp32:esp32@2.0.17` (pinned for legacy `driver/i2s.h` DMA stability).
- **Arduino Libraries:** `arduinoFFT` (`2.0.4`), `DHT sensor library` (`1.4.7`), `Adafruit Unified Sensor` (`1.1.15`), `U8g2` (`2.36.19`).
- **FQBN (Right `UART` Port):** `esp32:esp32:esp32s3:CDCOnBoot=default,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=115200`
- **FQBN (Left `USB` Port):** `esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=115200`
- **Python Pipeline:** Pure `numpy`, `pandas`, `scipy` (`ml_pipeline/build_v6_real_only_dataset.py` + `p2_humbugdb_q1_training_and_snr_eval.py`).

---

## 10. 5-Minute Live Supervisor Demo & Real Lab Testing Playbook

### 10.1 Boot Verification & Physical Button Controls
- **Power-On Self-Test:** Sequential LED blink (`Green 40 -> Yellow 41 -> Red 42`, `220 ms` each) + OLED splash **`WingBeat v6.0` / `100% Real QAT`**.
- **Buttons:** `BTN1` (`38`) = `1.5 s` Fan + LED self-test; `BTN2` (`39`) = OLED Main/Diagnostics toggle; `BTN3` (`1`) = Serial toggle; `BTN4` (`2`) = Reset NVS counters to `0`.

### 10.2 Testing with Live Captured Mosquitoes (Lab Protocol)
1. Capture `3–5` live mosquitoes inside a small transparent cup covered with thin mosquito-net mesh (zero solid plastic barrier blocking sound).
2. Place the trap in a quiet room (ceiling fan OFF near the bench so `FLR` settles to `~160–220`).
3. Hold the mesh face `2–6 cm` from the `INMP441` microphone port and gently tap the cup to induce flight — observe `[P2v6.0]` telemetry (`F0`, `SNR`, `RMS`, `FLR`, `RH2`, `RH3`, `STK`) on `115200` baud Serial Monitor and OLED counters.

### 10.3 Testing with Real Uncut Online Audio (Any Android / iPhone / Laptop)
Open **https://mlpipeline-henna.vercel.app** (`49` real uncut `44.1 kHz` WAVs, `0` synthetic tones, `0` baked loops), tap **`Loop: ON`**, hold ANY smartphone or laptop speaker **`5–12 cm`** from `INMP441` at **`70–90%`** volume, and play Clips `01` through `06`.

---

## 11. Scientific Limitations, Honest Disclosures & Phase-2 Roadmap

1. **Offline Real-Dataset Validation vs. Live Entomological Field Trials:** All `13,256` training/validation frames are 100% untouched real recordings from 5 verified datasets, and live board health (`[P2v6.0]`) is verified on `COM5`. Full live caged-mosquito entomological capture-rate statistics across wild species are explicitly scoped as **Phase-2** field trials.
2. **Honest Inter-Genus Overlap (`87.08%` 6-Class vs. `94.42%` Tier-1 Guild):** We NEVER claim `100%` species separation from wingbeat `f0` alone; natural wingbeat overlap between *Aedes*, *Anopheles*, and *Culex* is documented transparently in the clip-disjoint confusion matrix (Section 8.3).
3. **Honest ESC-50 Siren Boundary (`135 / 136`):** One continuous harmonic emergency siren (`1-54084-A-42.wav`) in ESC-50 overlaps female mosquito harmonic structure — reported honestly rather than artificially suppressed via overfitting.
4. **Honest Heater Disclosure (`0.25 W`):** The `100 Ohm 5W` resistor on `5V` yields `0.25 W` (`+2.5°C`), a breadboard-safe mild convective cue rather than a `37°C` skin emulator.
5. **Phase-2 Roadmap:** Live caged mosquito capture trials, `12V` PTC skin-temp heater + `CO2` lure, custom 2-layer PCB + 3D-printed acoustic funnel, and LoRaWAN/Wi-Fi dengue surveillance mapping.

---

## 12. Teammate Division of Roles & 15 High-Impact Viva Q&A

### 12.1 Recommended Presentation Split (3-Member Team)
- **Md. Najmus Sakib Rahatul (`1119003`) — System Architecture, Transfer-Function-Invariant DSP & TinyML:** Presents `130 Hz` HPF, Local-Envelope Spectral Whitening `R[k]`, Clip-Disjoint GroupCV benchmarks, and live firmware demo.
- **Nesar Uddin Ifag (`1119004`) — Hardware Interfacing, Dual-Rail Power & Sensor Isolation:** Presents the `2,757 BDT` BOM, 44-pin `ESP32-S3` wiring, `3.3V` vs. `5Vin` isolation, `220 Ohm` LED/UV math, `100 Ohm` ceramic resistor safety, and Active-LOW Relay switching.
- **Rayhan Alam Towhid (`1119007`) — Optical Gates, Enclosure CAD, Literature Review & Pollinator Safety:** Presents the 5 research gaps, constricted `4–5 cm` PVC funnel geometry, `100%` Bee Safe dual-layer veto, and Phase-2 roadmap.

### 12.2 15 High-Impact Supervisor & External Examiner Viva Q&A

1. **Q: Why didn't you run the suction fan continuously like commercial traps?**  
   **A:** A running `5V` brushless fan produces strong `150–350 Hz` mechanical + wind noise that masks faint `30–40 dB SPL` mosquito wingbeats. Keeping the fan `0%` OFF during every `64 ms` listening window preserves clean acoustic SNR and lowers average power draw.
2. **Q: Why did you add a `130 Hz` Butterworth HPF and Local-Envelope Spectral Whitening (`R[k]`) in `v6.0`?**  
   **A:** Raw lab recordings contain `50–125 Hz` chamber AC rumble, which causes naive classifiers to learn a "Clever Hans" lab-rumble shortcut and fail in quiet rooms or on phone speakers. The `130 Hz` HPF strips mains/AC/ceiling-fan rumble, while `R[k] = X[k]/flank_env[k]` mathematically cancels smooth microphone, funnel, Android, iPhone, or laptop EQ tilt (`22/24` features invariant to `<= 0.03`).
3. **Q: Did you use any synthetic sine waves or fake generated data to train your model?**  
   **A:** Zero. `WingBeat-TinyNet v6.0 Clean-Real` is trained on **`13,256` 100% untouched real frames** (`0` synthetic `np.sin()` waveforms, `0` artificial EQ/noise/pitch augmentations) and evaluated with **Strict Clip-Disjoint 5-Fold GroupCV** so zero overlapping frames leak between training and validation.
4. **Q: Why use `ESP32-S3` instead of an Arduino Uno (`ATmega328P`)?**  
   **A:** Arduino Uno has only `2 KB` SRAM and no native I2S hardware peripheral. `ESP32-S3` provides native I2S DMA, dual-core `240 MHz` DSP, `512 KB` internal SRAM, NVS Flash, and runs our full pipeline in `< 2.0 ms` using only `11%` Flash and `10%` RAM.
5. **Q: How does your trap work across winter (`18°C`) and Bangladeshi summer (`38°C`)?**  
   **A:** `DHT22` (`GPIO 7`) measures live ambient `T_amb` every `2.5 s` and executes Universal Thermal Spectral Bin Warping (`bsh = round(7.0*(clamp(T_amb,10,45)-28.0)/15.625)`), aligning `f0`, centroid, and all 6 biological subbands onto the `28°C` reference grid automatically.
6. **Q: Can your trap separate *Aedes*, *Anopheles*, and *Culex* with 100% accuracy?**  
   **A:** No — we honestly report **`87.08%`** clip-disjoint 6-class accuracy (`94.42%` Tier-1 Female Vector Guild) because biological wingbeat frequencies naturally overlap across genera. Tier-1 Guild gates the fan (`96.15–98.62%` vector containment), while 4-frame temporal voting estimates genus counts for surveillance.
7. **Q: How do you guarantee bees and pollinators are never killed, even when a bee's `2×f0` harmonic falls in the mosquito band?**  
   **A:** Three layers: (1) Unified `0.5×/1.5×` sub-octave recovery snaps `2×f0`-dominant bees (`340–570 Hz`) right back to their true `140–285 Hz` fundamental, (2) Clip-Disjoint GroupCV achieved **`100.00%` Bee Safe (`0 / 1,613` leaked)** across real Wikimedia Commons outdoor bees, and (3) `HW-201 Gate #1` provides an independent optical transit-width veto (`26–60 ms` -> `BEE SAFE`).
8. **Q: How can a quiet live mosquito (`2–6 cm` away) trigger the trap when a room has background noise?**  
   **A:** The `130 Hz` HPF cuts room ceiling-fan RMS by `>2×`, and an exponential moving average (`ambient_rms_ema`) smoothly lowers `dyn_rms_floor` down to `MIN_ACTIVE_RMS = 160` in quiet rooms while raising it automatically in loud rooms.
9. **Q: What stops human speech, `"mew-mew"` humming, or room noise from triggering the fan?**  
   **A:** Four layers: adaptive `RMS` + `11.5 dB` local `SNR` gate, `24`-feature QAT INT8 MLP (`97.03%` single-frame noise rejection), whitened chitin harmonic guards (`RH2 >= 2.2`, `RH3 >= 2.0`, `SUB <= 0.58`), and a 4-frame pitch-locked anchor streak (`|Δf0| <= 38 Hz`, `|f0 - anchor| <= 46 Hz`).
10. **Q: Why use QAT Symmetric INT8 quantization instead of floating-point weights?**  
    **A:** Straight-Through QAT INT8 cuts weight storage `4×` to **`7.18 KB`** with virtually zero accuracy drop (`87.08%` INT8 vs. `87.09%` FP32; Macro-F1 `83.22%` vs. `83.21%`) and needs zero TFLite Micro dependency.
11. **Q: Why put `220 Ohm` resistors on every LED and keep UV LEDs off GPIO pins?**  
    **A:** Keeps status LEDs at safe `~5.9 mA` per GPIO (`< 20 mA` limit) and powers all 6 UV LEDs (`~49 mA` total) safely from `5Vin`.
12. **Q: What happens if room glare keeps an `HW-201` IR gate stuck `LOW`?**  
    **A:** If `LOW` persists `> 150 ms` (`IR_STUCK_MS`), firmware flags `IR:POT` and ignores the stuck pin so `Cat` never counts falsely and acoustic gating continues uninterrupted.
13. **Q: Why is the `100 Ohm 5W` ceramic resistor safe on a plastic breadboard?**  
    **A:** At `5V`, $P = V^2/R = 25/100 = 0.25\text{ W}$ (`5%` of its `5W` rating, `+2.5°C` warmth).
14. **Q: Why pin `esp32:esp32@2.0.17` instead of ESP32 Core v3.x?**  
    **A:** Core `v2.0.17` preserves stable legacy `driver/i2s.h` DMA APIs with full `ESP32-S3 N16R8` OPI PSRAM support.
15. **Q: Do OLED counts survive power loss?**  
    **A:** Yes — all 6 counters (`AED`, `ANO`, `CUL`, `Bee`, `Mal`, `Cat`) persist in `ESP32-S3` NVS Flash (`Preferences`) until reset via `BTN4`.

---

## 13. References

1. **Villarreal, S. M., Winokur, O., & Harrington, L. (2017).** The impact of temperature and body size on fundamental flight tone variation in the mosquito vector *Aedes aegypti*. *Journal of Medical Entomology*, 54(5), 1116–1121. `doi:10.1093/jme/tjx079`
2. **Mukundarajan, H., et al. (2017).** Using mobile phones as acoustic sensors for high-throughput mosquito surveillance (*Abuzz*). *eLife*, 6, e27854. `doi:10.7554/eLife.27854`
3. **Kiskin, I., et al. (2021).** *HumBugDB*: A large-scale acoustic mosquito dataset. *NeurIPS Datasets & Benchmarks Track*, 34.
4. **Supratak, A., Phuphisut, O., Chumphon, T., & Limcharoen, T. (2024).** *MosquitoSong+*: A noise-robust deep learning model for mosquito classification from wingbeat sounds. *PLOS ONE*, 19(10), e0310121. `doi:10.1371/journal.pone.0310121`
5. **Piczak, K. J. (2015).** ESC: Dataset for environmental sound classification (*ESC-50*). *ACM Multimedia*, 1015–1018. `doi:10.1145/2733373.2806390`
6. **Potamitis, I., Eliopoulos, P., & Rigakis, I. (2017).** Automated remote insect surveillance at a global scale and the Internet of Things. *Robotics*, 6(3), 19.
7. **Espressif Systems (2024).** *ESP32-S3 Technical Reference Manual (v1.5)* — I2S Peripheral, NVS, and GPIO Matrix Specifications.
