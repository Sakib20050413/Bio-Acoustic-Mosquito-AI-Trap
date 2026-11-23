# Antigravity-তে শুরু করার গাইড (Zero-Hassle Handoff)

## ১. ফোল্ডার ওপেন ও অটো-সেটআপ
1. এই ZIP ফাইলটি আনজিপ করে **`Project2_CSE326_Antigravity_Workspace`** ফোল্ডারটি সরাসরি **Antigravity IDE**-তে `Open Folder` দিয়ে খুলুন।
2. Antigravity-এর বিল্ট-ইন টার্মিনালে এই এক লাইনের কমান্ড চালালেই আপনার `~/Arduino/` ফোল্ডারে ৩টি কোড ফোল্ডার রেডি হয়ে যাবে:
   ```bash
   bash scripts/setup_ubuntu_arduino.sh
   ```

## ২. Antigravity চ্যাটে প্রথম মেসেজ হিসেবে এটি কপি-পেস্ট করুন
```text
Read GEMINI.md and PROJECT_MEMORY.md in this workspace first. Lock context strictly to Project 2 (CSE 326 Bio-Acoustic & Optical Mosquito Trap on ESP32-S3 N16R8). Never mix Project 1 (CSE 301). Always reply in Bengali script (বাংলা) with English technical terms. Confirm you loaded our hardware pinout, firmware folders, and current bring-up status, then help me continue from Step 1 of PROJECT_MEMORY.md.
```

## ৩. এই ওয়ার্কস্পেসে যা যা আছে
- `GEMINI.md` & `.antigravity/rules.md` — অটো-লোডেড প্রজেক্ট কনটেক্সট, পিনআউট ও Q1 নিয়মাবলী।
- `PROJECT_MEMORY.md` — এখন পর্যন্ত কতদূর কাজ হয়েছে এবং পরের ধাপ কী।
- `firmware/` — Arduino IDE-র জন্য ৩টি রেডি ফোল্ডার (`p2_hardware_diag`, `p2_acoustic_capture_esp32s3`, `p2_deployment_trap_esp32s3` + `wingbeat_q1_int8.h`)।
- `ml_pipeline/` — HumBugDB SNR ও INT8 কোয়ান্টাইজেশন Python স্ক্রিপ্ট ও `requirements.txt`।
- `docs/` & `diagrams/` — সব PDF গাইড, ৪-পেজ হার্ডওয়্যার ম্যানুয়াল, নতুন সম্পূর্ণ ওয়্যারিং ইনফোগ্রাফিক, হলুদ বাটন গাইড এবং ইন্টারঅ্যাক্টিভ `Interactive_ESP32S3_44Pin_Wiring.html`।
- `scripts/setup_ubuntu_arduino.sh` — এক কমান্ডে Ubuntu + Arduino ফোল্ডার সেটআপ স্ক্রিপ্ট।
