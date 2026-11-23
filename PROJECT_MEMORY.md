# PROJECT_MEMORY.md — Live Hardware & Software State (2026-11-24 — v6.0 Clean-Real)

## Completed Milestones
- [x] **Physical Hardware Live-Verified (`COM5`, `ESP32-S3 N16R8` rev v0.2, MAC `28:84:85:49:17:50`):**
  - All 12 hardware groups (`2,757 BDT`) assembled: `ESP32-S3 N16R8` (44-pin), `INMP441` (`GPIO 4/5/6`, `3V3`), `DHT22` (`GPIO 7`, `3V3`), `1.3" SH1106 OLED` (`GPIO 8/9`, `0x3C`), `2× HW-201 IR` (`GPIO 10/14`), `5V` Active-LOW Relay (`GPIO 13`) + Fan, `3× 10mm` Status LEDs (`GPIO 40/41/42`, `+220 Ω`), `4×` Yellow Buttons (`GPIO 38/39/1/2`), `6× 395nm UV LEDs` (`+220 Ω` each on `5Vin`), and real `100 Ω 5W` Ceramic Thermal Resistor (`0.25W` / `+2.5°C` on `5Vin`).
  - **Live Hardware Verification (`v6.0 Clean-Real` flashed to `COM5`, `Hash of data verified.`):** `[P2v6.0] ACT/IDL ... T:24.1-24.2C T1:0 P:0.90-1.00 STK:0 RMS:423-685 FLR:391-588 IR:OK/OK` — `130 Hz` Butterworth HPF cut room ceiling-fan RMS by `>2×`, adaptive floor drops to `160` in quiet rooms for faint live mosquitoes (`2–6 cm`), `0` false room triggers. *(Live acoustic speaker & live captured mosquito bench verification awaiting Sakib's physical test.)*
- [x] **WingBeat-TinyNet v6.0 Clean-Real — 0% Synthetic / 0% Artificial Aug + Transfer-Function-Invariant DSP (`24 -> 96 -> 48 -> 6`, `7,350` params / `7.18 KB` QAT INT8):**
  - **100.0% Untouched Real Dataset (`ml_pipeline/build_v6_real_only_dataset.py` -> `humbugdb_real_features.csv`):** `n = 13,256` untouched real frames across `52` source groups & `1,580` unique clips/blocks from 5 real datasets (*PLOS ONE MosquitoSong+*, *Oxford HumBugDB*, *ESC-50* 17 cats, *Wikimedia Commons* real outdoor bees, *Cumilla* real room/vocal/sawmill recordings). **ZERO synthetic `np.sin()` rows, ZERO artificial `_CHAN` / `_SNR` / `_aug` / iPhone-EQ filter rows!**
  - **Root-Cause DSP Breakthrough (Solving Lab-Rumble Shortcut & Device-Specific EQ Drift):**
    1. Exact `130 Hz` 2nd-order Butterworth HPF (`HP_B0..HP_A2`, bit-exact C++/Python) strips Thailand lab `50–125 Hz` AC rumble & Bangladesh ceiling-fan motor rumble.
    2. **Local-Envelope Spectral Whitening (`R[k] = spec[k] / flank_mean[k]`)** mathematically cancels ANY smooth acoustic transfer function `G(f)` across open-air `INMP441`, 3D PVC funnel, Android, iPhone, and laptop speakers (`22/24` features invariant to `<= 0.03` between flat mic and `720 Hz` HPF speaker!).
    3. **Unified `0.5× / 1.5×` Sub-Octave Fundamental Recovery** in whitened space `R[k]` simultaneously snaps `2×f0`-dominant real bees back to `135–290 Hz` (`BEE SAFE`) AND small-speaker `2×f0`-peaking females back to `340–660 Hz`.
    4. **Universal `DHT22` Thermal Spectral Bin Warping (`+7.0 Hz/°C` ref `28.0°C`)** across `-10..+50°C` live display (`10..45°C` biological clamp).
  - **Honest Leak-Free Clip-Disjoint 5-Fold GroupCV (`n = 13,256`, zero frame overlap between train/val):**
    - `WingBeat-TinyNet v6.0 INT8`: **`87.08%`** 6-Class Acc | **`83.22%`** Macro-F1 | **`94.42%`** Tier-1 Guild | **`100.00%`** Bee Safe (`0/1,613` leaked) | **`97.03%`** Noise Rej (FP32: `87.09%` / `83.21%` -> zero INT8 drop).
    - Per-Class Recall (INT8 GroupCV): `Noise/Env 92.79%`, `Pollinator 93.37%`, `Aedes 74.30%`, `Anopheles 70.62%`, `Culex 79.18%`, `Male 96.49%`.
    - Clip-Disjoint Holdout (`n = 2,658`, Champion `seed=106, lr=0.0095, l2=1.8e-4`, logged in `ml_pipeline/p2_export/v6_clean_real_training_log.json`): **`87.17%`** Acc | **`83.46%`** Macro-F1 | **`93.98%`** Guild | **`100.00%`** Bee Safe | **`96.55%`** Noise Rej.
  - **Offline Real-Recording Firmware Audit (`v6.0` `CFG_B`: `streak=4`, `p_vec>=0.72`, `RH2>=2.2`, `RH3>=2.0`, `SUB<=0.58`, `MIN_ACTIVE_RMS=160`):**
    - Negatives: `0` Fan triggers on `VID2_mew_mew` (`step=512`), `VID1_talk`, `Room_1138_sawmill`, `Room_user_voice`, `Bee_Hummel` (`44.1s`, `240` Bee hits), `Bee_buzzing` (`20.1s`, `243` Bee hits), `Demo_04_Bee`, `Demo_05_Male` (`49` Male hits), `Demo_06_ESC50`; **`135 / 136` (`99.26%`)** ESC-50 WAVs zero false triggers (honest single siren clip `1-54084-A-42.wav`).
    - Positives (Quiet Live-Air `RMS=260` / Android `550 Hz` / Small Phone `720 Hz` / Tiny Speaker `950 Hz`): `01_Aedes`: `20 / 21 / 21 / 21`; `02_Anopheles`: `13 / 16 / 16 / 15`; `03_Culex`: `20 / 28 / 26 / 27`.
    - Full `43`-track real library audit: **`20 / 20`** female vector WAVs triggered, **`23 / 23`** non-vector WAVs zero false fan triggers!
  - **Deployed `v6.0` Checksums (4/4 header copies synced):**
    - `wingbeat_q1_int8.h`: `SHA-256: C367F46DF8F904ADFF363B0320DE36DE4987EE83A59A9D407518744375E6A666` (`33,122 B`, `7.18 KB` INT8 weights)
    - `p2_deployment_trap_esp32s3.ino`: `SHA-256: 434DB5CC1790553F040178A5BB6451A9A3AC791B019C797A54319F4C5E1EF715` (`27,940 B`)
    - `UART` binary: `356,573 B` sketch / `356,944 B` `.bin` (`11%` Flash, `35,144 B` / `10%` RAM) — **flashed to live `COM5`**
    - `USB` binary: `345,073 B` sketch / `345,440 B` `.bin` (`10%` Flash, `34,936 B` / `10%` RAM)
- [x] **Live Production Vercel Audio Hub (`https://mlpipeline-henna.vercel.app`, `web_demo/`):**
  - `49` uncompressed `44.1 kHz` 100%-real unlooped WAVs (`6` quick demo + `43`-track real library from *MosquitoSong+*, *Oxford HumBugDB 44.1 kHz*, *Wikimedia Commons Real Bees*, *ESC-50*, *Sakib Room/Mew*), updated to `v6.0 Clean-Real` (`0` synthetic tracks, `0` baked loops), plus `/3d-enclosure` and `/wiring-44pin`.
- [x] **DIY 4" PVC Pipe CAD (`.OBJ`/`.STL`, Interactive 3D Viewer, `70×90 mm` PCB Blueprint), `24s` HD Explainer Video, Updated `v6.0` Complete Academic Project Book PDF, Portable Master ZIP & GitHub Main Sync.**
