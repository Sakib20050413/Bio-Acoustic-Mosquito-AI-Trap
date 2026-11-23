# Project 2 (CSE 326) — WingBeat-TinyNet v6.0 Clean-Real Portable All-in-One Master Pack

**Unpack on ANY Windows 10/11 or Ubuntu Linux PC — 100% Real Data, Zero Synthetic Audio!**

| Field | Value |
| :--- | :--- |
| **Course** | CSE 326 — Microprocessor / Embedded Systems Sessional (BAIUST, Cumilla) |
| **Team** | Md. Najmus Sakib Rahatul (`1119003`), Nesar Uddin Ifag (`1119004`), Rayhan Alam Towhid (`1119007`) |
| **Supervisor** | Md. Fahim Kabir Chowdhury (Lecturer, Dept. of CSE, BAIUST) |
| **Model & Firmware** | `WingBeat-TinyNet v6.0 Clean-Real` (`130 Hz` Butterworth HPF + Local-Envelope Spectral Whitening `R[k]` + QAT 6-Class INT8 `24->96->48->6` + Universal `DHT22` Thermal Bin Warping) |
| **Dataset Integrity** | **`13,256` untouched real frames (`5` real datasets) — `0` synthetic `np.sin()` rows, `0` artificial EQ/noise/pitch augmentations** |
| **Verified Checksums** | `wingbeat_q1_int8.h` (`SHA-256: C367F46DF8F904AD...`), `p2_deployment_trap_esp32s3.ino` (`SHA-256: 434DB5CC1790553F...`) |

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
- Pre-compiled `v6.0` `.bin` files for both `uart/` (`356,944 B`) and `usb/` (`345,440 B`) are also ready in `compiled_bins/p2_deployment_trap_esp32s3/`.

### Step 3: Live Lab Testing — Real Mosquitoes OR Any Phone / Laptop Speaker
- **Live Captured Mosquito Test (Quiet Room / Mesh Cup):** Turn off ceiling fans near the bench so the adaptive `FLR` settles down toward `160–220`, then hold the fine mesh cup containing live mosquitoes **`2–6 cm`** from `INMP441`.
- **Online Speaker Demo (No Laptop Needed):** Open **https://mlpipeline-henna.vercel.app** on **ANY Android phone, iPhone, or laptop**, tap `Loop: ON`, and hold the speaker **`5–12 cm`** from `INMP441` at **`70–90%`** volume (or run `python -u scripts/phone_cam_bridge.py` locally on port `8765`):
  - `01_Aedes_aegypti_Dengue_Chikungunya.wav` -> OLED **`AEDES (DENGUE)!`**, Red LED + Fan `5.0 s`, `AED +1`
  - `02_Anopheles_dirus_Malaria.wav` -> OLED **`ANOPH (MALARIA)`**, Red LED + Fan `5.0 s`, `ANO +1`
  - `03_Culex_quinquefasciatus_Filariasis.wav` -> OLED **`CULEX (FILAR.)`**, Red LED + Fan `5.0 s`, `CUL +1`
  - `04_Pollinator_Honeybee_Spared.wav` -> OLED **`BEE SAFE`** (Real Wikimedia Commons bee), Green LED `1.5 s`, **Fan OFF**, `Bee +1`
  - `05_Male_Mosquito_Ignored.wav` -> OLED **`MALE IGN`**, Yellow LED `1.5 s`, **Fan OFF**, `Mal +1`
  - `06_ESC50_CarHorn_Birds_Sawmill_Noise.wav` -> OLED **`NOISE IGN`**, All LEDs OFF, **Fan OFF**, `0` false triggers!

### Ubuntu Linux Quick Setup & Retrain `v6.0 Clean-Real`
```bash
bash scripts/setup_ubuntu_arduino.sh
python3 -m venv .venv && source .venv/bin/activate && pip install -r ml_pipeline/requirements.txt
python3 ml_pipeline/p2_humbugdb_q1_training_and_snr_eval.py --csv ml_pipeline/humbugdb_real_features.csv --out ml_pipeline/p2_export --rounds 6
```

---

## Complete Archive Contents

| Folder / File | What's Inside |
| :--- | :--- |
| **`docs/Project2_CSE326_Complete_Project_Book.pdf`** | **Complete 13-Section Print-Ready Academic Project Book (B&W A4 PDF, `v6.0 Clean-Real`)** — Honest Clip-Disjoint 5-Fold GroupCV metrics, `130 Hz` HPF + Spectral Whitening math, BOM, 44-pin wiring, 6×6 confusion matrix, lab test playbook & 15 Viva Q&A |
| **`docs/Project2_CSE326_Complete_Project_Book.md`** | Editable Markdown source of the Complete Project Book |
| **`firmware/`** | All 3 verified Arduino sketches (`p2_deployment_trap_esp32s3` `v6.0` + `wingbeat_q1_int8.h`, `p2_hardware_diag`, `p2_acoustic_capture_esp32s3`) |
| **`compiled_bins/`** | Pre-compiled `v6.0` ESP32-S3 `.bin` binaries (`uart/` & `usb/`) for instant flashing without recompiling |
| **`ml_pipeline/`** | `build_v6_real_only_dataset.py`, `p2_humbugdb_q1_training_and_snr_eval.py`, 0%-synthetic `13,256`-frame `humbugdb_real_features.csv`, `p2_export/v6_clean_real_training_log.json`, `wingbeat_q1_int8.h` |
| **`web_demo/` & `demo_audio/`** | `49` uncompressed `44.1 kHz` 100%-real unlooped WAV files (*MosquitoSong+*, *Oxford HumBugDB*, *Wikimedia Bees*, *ESC-50*) + Vercel static app |
| **`enclosure_cad/` & `media/`** | Interactive 3D PVC Enclosure Viewer, `.OBJ`/`.STL` CAD, `70×90 mm` PCB layout, 3 HD blueprints & `24 s` explainer video |
| **`diagrams/` & `scripts/`** | Interactive 44-pin HTML wiring diagram, `home_pc_full_setup.ps1`, `phone_cam_bridge.py`, `setup_ubuntu_arduino.sh`, `read_diag.ps1` |
| **`GEMINI.md` / `PROJECT_MEMORY.md`** | Workspace rules & complete `v6.0 Clean-Real` state |
