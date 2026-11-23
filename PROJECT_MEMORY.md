# PROJECT_MEMORY.md — Live Hardware & Software State (2026-11-23 — FINAL v5.0)

## Completed Milestones
- [x] **Physical Hardware 100% Live-Verified (`COM5`, `ESP32-S3 N16R8`):**
  - All 12 hardware groups (`2,757 BDT`) assembled with real `100 Ohm` thermal resistor, `6× 395nm UV LEDs` (`220 Ω` each), `3× 10mm` Status LEDs, `4×` Yellow Buttons, `DHT22`, `INMP441`, `SH1106 OLED`, `2× HW-201 IR`, `5V` Active-LOW Relay + Fan.
  - **Live Hardware Bench Proof (NVS Flash persisted on Sakib's real board!):** `AED:8 ANO:1 CUL:8 BEE:5 MAL:6 CAT:1`, `T:28.7C–28.8C`, `IR:OK/OK`, and `100%` rejection of loud room speech (`RMS 10,051`, `T1:0`).
- [x] **WingBeat-TinyNet v5.0 — 10-Round Multi-Seed QAT INT8 Champion (`24 -> 96 -> 48 -> 6`, `7,350` params / `7.18 KB` INT8):**
  - 5-Fold CV (`n = 15,402` real + channel-augmented frames): **INT8 `94.16%` (beats FP32 `94.04%`!) | Macro-F1 `91.34%` | Tier-1 Guild `98.01%` | Bee Safe `100.00%` (`0/1,100`) | Noise Rej `99.19%`**.
  - 10-Round QAT Champion (`seed=101`, `Holdout n=2,310`, logged in `ml_pipeline/p2_export/v5_10round_training_log.json`): **INT8 `94.98%` | Macro-F1 `92.76%` | Tier-1 Guild `98.31%` | Bee Safe `100.00%` | Noise Rej `99.40%`**.
  - Wide-Climate `DHT22` Thermal Spectral Bin Warping (`-10°C..+50°C` live display, `[10°C, 45°C]` biological warp): `14°C` `44.1%->64.9%`, `18°C` `37.1%->73.3%`, `28°C` `95.5%/100.0%`, `34°C` `37.6%->94.6% (+57.0%)`, `38°C` `19.3%->86.1% (+66.8%)`, `42°C` `15.8%->89.1% (+73.3%!)`.
  - Exact Firmware Cadence Audit: **ESC-50 17 Categories (`136` WAVs, `8,981` frames) = `0` False Fan Triggers (`0.00%` FAR)**; Sakib Room (`1,715` frames) = **`0` False Fan Triggers**; **6/6 Demo WAVs (`44.1 kHz`) PASS** under both Clean & iPhone Speaker + Room Hum!
  - Deployed Checksums: `wingbeat_q1_int8.h` (`SHA-256: 4e0d124437b052c9`, `33,111 B`, 4/4 copies synced), `p2_deployment_trap_esp32s3.ino` (`SHA-256: 16d428ef8e806d92`, `24,009 B`), `UART` `355,073 B` (`11%` Flash, `9%` RAM) flashed to `COM5` + `USB` `343,573 B` (`10%`).
- [x] **DIY 4" PVC Pipe + 2" Constricted Throat 3D CAD, Compact `70×90 mm` Perfboard PCB & HD Video:**
  - `enclosure_cad/WingBeat_v5_PVC_Trap_Assembly.obj` (`14` named Blender groups) & `.stl`, `enclosure_cad/Interactive_3D_PVC_Enclosure_Viewer.html`, `enclosure_cad/DIY_PVC_and_Compact_PCB_Build_Guide.md`, 3× `1920×1080` blueprint renders in `enclosure_cad/renders/`, and `media/WingBeat_TinyNet_v5_PVC_Suction_Explainer.mp4` (`24s`, `1280×720` H.264 + real *Aedes*/Honeybee AAC audio).
- [x] **25-Paper DOI-Verified Q1 Literature & Valuation Report, Updated Complete Project Book PDF, CC BY-SA 4.0 + MIT Licenses, GitHub Push (`Sakib20050413/Bio-Acoustic-Mosquito-AI-Trap`) & Portable Master ZIP.**
