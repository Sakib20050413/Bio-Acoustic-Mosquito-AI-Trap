# Project 2 (CSE 326) — WingBeat-TinyNet v4 Portable All-in-One Master Pack

**Unpack on ANY Windows 10/11 or Ubuntu Linux PC — Everything Included!**

| Field | Value |
| :--- | :--- |
| **Course** | CSE 326 — Microprocessor / Embedded Systems Sessional (BAIUST, Cumilla) |
| **Team** | Md. Najmus Sakib Rahatul (`1119003`), Nesar Uddin Ifag (`1119004`), Rayhan Alam Towhid (`1119007`) |
| **Supervisor** | Md. Fahim Kabir Chowdhury (Lecturer, Dept. of CSE, BAIUST) |
| **Model & Firmware** | `WingBeat-TinyNet v5.0` (10-Round QAT 6-Class INT8 `24->96->48->6` + Universal `DHT22` `-10..+50°C` Thermal Bin Warping) |
| **Verified Checksums** | `wingbeat_q1_int8.h` (`SHA-256: 4e0d124437b052c9`), `p2_deployment_trap_esp32s3.ino` (`SHA-256: 16d428ef8e806d92`) |

---

## Quick 3-Step Run on ANY Windows 10/11 PC

### Step 1: Extract & Run 1-Click Auto-Setup
Unzip `Project2_CSE326_Final_Portable_Master_Pack.zip`, open PowerShell inside the extracted folder, and run:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\home_pc_full_setup.ps1
```
This automatically installs `arduino-cli` (if needed), pins `esp32:esp32@2.0.17` + 4 Arduino libraries (`arduinoFFT`, `DHT sensor library`, `Adafruit Unified Sensor`, `U8g2`), copies all 3 sketches to `Documents\Arduino\`, creates Python `.venv` (`numpy`, `pandas`, `scipy`), and opens a **1-click interactive menu** (`Diag`, `Capture`, `Final Trap`, `Serial Monitor`, `Demo Audio Server`, `ML Smoke Test`).

### Step 2: Plug in `ESP32-S3 N16R8` (Right `UART` USB-C Port)
- Holding the board upright (metal RF shield at top): plug USB-C into the **Right `UART` (`CH343`)** port.
- Choose Menu Option `[3]` (uses `CDCOnBoot=default`, `16MB Flash`, `OPI PSRAM`, `115200` baud).
- Pre-compiled `.bin` files for both `uart/` and `usb/` are also ready in `compiled_bins/p2_deployment_trap_esp32s3/`.

### Step 3: Run 5-Minute Live 6-Clip Demo (Supervisor Presentation)
1. Choose Menu Option `[5]` (or run `python -u scripts/phone_cam_bridge.py`) to start the Mobile Demo Audio Server on port `8765`.
2. Open `http://<YOUR-PC-IP>:8765/demo` on any smartphone on the same Wi-Fi/hotspot (or play the 6 WAVs directly from `demo_audio/`).
3. Hold the phone speaker **`5–10 cm`** from `INMP441` at **`70–85%`** volume and play Clips `01` through `06`:
   - `01_Aedes_aegypti_Dengue_Chikungunya.wav` -> OLED **`AEDES (DENGUE)!`**, Red LED + Fan `5.0 s`, `AED +1`
   - `02_Anopheles_dirus_Malaria.wav` -> OLED **`ANOPH (MALARIA)`**, Red LED + Fan `5.0 s`, `ANO +1`
   - `03_Culex_quinquefasciatus_Filariasis.wav` -> OLED **`CULEX (FILAR.)`**, Red LED + Fan `5.0 s`, `CUL +1`
   - `04_Pollinator_Honeybee_Spared.wav` -> OLED **`BEE SAFE`**, Green LED `1.5 s`, **Fan OFF**, `Bee +1`
   - `05_Male_Mosquito_Ignored.wav` -> OLED **`MALE IGN`**, Yellow LED `1.5 s`, **Fan OFF**, `Mal +1`
   - `06_ESC50_CarHorn_Birds_Sawmill_Noise.wav` -> OLED **`NOISE IGN`**, All LEDs OFF, **Fan OFF**, `0` false triggers!

### Ubuntu Linux Quick Setup
```bash
bash scripts/setup_ubuntu_arduino.sh
python3 -m venv .venv && source .venv/bin/activate && pip install -r ml_pipeline/requirements.txt
python3 ml_pipeline/p2_humbugdb_q1_training_and_snr_eval.py --csv ml_pipeline/humbugdb_real_features.csv --out ml_pipeline/p2_export
```

---

## Complete Archive Contents

| Folder / File | What's Inside |
| :--- | :--- |
| **`docs/Project2_CSE326_Complete_Project_Book.pdf`** | **Complete 12-Chapter Print-Ready Academic Project Book (B&W A4 PDF)** — Supervisor brief, BOM, 44-pin wiring, Universal `DHT22` Thermal Spectral Bin Warping proof, Q1 ablations, 6×6 confusion matrix, ESC-50 audit, 5-min demo script & 15 Viva Q&A |
| **`docs/Project2_CSE326_Complete_Project_Book.md`** | Editable Markdown source of the Complete Project Book |
| **`docs/`** | Additional hardware wiring manual (`Project2_CSE326_Visual_Hardware_and_Breadboard_Wiring_Manual.pdf`), Q1 strict guide PDF, and handoff doc |
| **`firmware/`** | All 3 verified Arduino sketches (`p2_deployment_trap_esp32s3` + `wingbeat_q1_int8.h`, `p2_hardware_diag`, `p2_acoustic_capture_esp32s3`) |
| **`compiled_bins/`** | Pre-compiled ESP32-S3 `.bin` binaries (`uart/` & `usb/`) for instant flashing without recompiling |
| **`ml_pipeline/`** | `p2_humbugdb_q1_training_and_snr_eval.py`, real 4-dataset `15,402`-frame feature CSV (`humbugdb_real_features.csv`), `wingbeat_q1_int8.h`, `requirements.txt` |
| **`demo_audio/`** | All `6` verified continuous `44.1 kHz` `8s` live-demo WAV files (*MosquitoSong+* & *ESC-50*, phone-speaker calibrated) |
| **`diagrams/`** | Interactive 44-pin HTML wiring diagram + 4-page breadboard/funnel PNGs + infographic + yellow button guide |
| **`scripts/`** | `home_pc_full_setup.ps1`, `phone_cam_bridge.py` (with `/demo` mobile player), `setup_windows_arduino.ps1`, `setup_ubuntu_arduino.sh`, `read_diag.ps1` |
| **`GEMINI.md` / `PROJECT_MEMORY.md`** | Workspace rules & complete project state for instant Antigravity / AI resumption on any machine |
