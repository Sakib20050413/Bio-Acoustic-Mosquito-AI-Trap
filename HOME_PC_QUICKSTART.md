# Home PC Quickstart — Project 2 (CSE 326 WingBeat-TinyNet)

বাসার পিসিতে গিয়ে কোনো ঝামেলা ছাড়াই ১ ক্লিকে পুরো প্রজেক্ট রান করার সম্পূর্ণ গাইড।

---

## ১. এই ফোল্ডারটি বাসার পিসিতে নিন
- `Project2_CSE326_HomePC_Ready.zip` (বা পুরো `Project2_CSE326_Antigravity_Workspace` ফোল্ডার) পেনড্রাইভ / Google Drive দিয়ে বাসার পিসিতে কপি করে আনজিপ করুন।
- ফোল্ডারের ভিতরে **একটি মাত্র পাওয়ারশেল কমান্ড** চালালেই সব অটো-সেটআপ হয়ে যাবে:
  ```powershell
  powershell -ExecutionPolicy Bypass -File .\scripts\home_pc_full_setup.ps1
  ```

এটি নিজে থেকেই:
1. `arduino-cli` (না থাকলে) ইনস্টল করবে
2. `esp32:esp32@2.0.17` বোর্ড কোর + ৪টি লাইব্রেরি (`arduinoFFT`, `DHT sensor library`, `Adafruit Unified Sensor`, `U8g2`) ইনস্টল করবে
3. ৩টি sketch `Documents\Arduino\`-তে কপি করবে
4. Python `.venv` (`numpy`, `pandas`, `scipy`) রেডি করবে
5. COM পোর্ট অটো-ডিটেক্ট করে **১-ক্লিক মেনু** দেবে (`Diag`, `Capture`, `Final Trap`, `Serial Monitor`, `Phone Camera Bridge`, `ML Smoke Test`)

---

## ২. Pre-Compiled `.bin` ফাইল কোথায় আছে?
`compiled_bins/` ফোল্ডারে ৩টি ফার্মওয়্যারেরই রেডি `.bin` রাখা আছে (দুটি ভ্যারিয়েন্টেই):
- `compiled_bins/<sketch>/uart/` — **Right `UART` (`CH343`) USB-C পোর্টের জন্য** (`CDCOnBoot=default`)
- `compiled_bins/<sketch>/usb/` — **Left `USB` (Native OTG) USB-C পোর্টের জন্য** (`CDCOnBoot=cdc`)

> **মনে রাখবেন:** কেবলটি যদি **Right `UART` (`CH343`)** পোর্টে লাগান, অবশ্যই `uart/` বা `CDCOnBoot=default` ব্যবহার করবেন—নাহলে Serial Monitor ফাঁকা দেখাবে।

---

## ৩. ওয়্যারিং চেকলিস্ট (১ মিনিটে সব মিলিয়ে নিন)
1. **ব্রেডবোর্ড (`MB-102`):** পাশের `+`/`-` পাওয়ার রেল Row 30-এ মাঝখান থেকে কাটা থাকে—দুই পাশ জাম্পার দিয়ে **Bridge** করুন, এবং **Common `GND`** নিশ্চিত করুন।
2. **INMP441 Mic:** অবশ্যই **৬টি পিন সোল্ডার** করে লাগাবেন (`SCK=4`, `WS=5`, `SD=6`, `L/R=GND`, `VDD=3.3V`, `GND=GND`)।
3. **1.3" SH1106 OLED (`0x3C`):** পিন অর্ডার **`GND | VCC | SCL | SDA`** (`VCC=3.3V`, `SCL=9`, `SDA=8`)।
4. **DHT22 (`V182`):** `+ = 3.3V`, `out = GPIO 7`, `- = GND`।
5. **2x HW-201 IR Gate:** `Gate1 OUT = GPIO 10`, `Gate2 OUT = GPIO 14`, `VCC = 3.3V`, `GND = GND`।
6. **5V Relay (`JQC3F-05VDC-C`) + Fan:** `IN = GPIO 13`, `VCC = 5Vin`, `GND = GND`, `COM = 5Vin`, `NO = Fan(+)`, `Fan(-) = GND`। (বুট হতেই ফ্যান ঘুরলে কোডের `#define RELAY_ACTIVE_HIGH true` পরিবর্তন করে `false` করুন)।
7. **3x Status LEDs (`+220 Ohm`):** Green=`GPIO 40`, Yellow=`GPIO 41`, Red=`GPIO 42`।
8. **4x Yellow Buttons (to `GND`):** `GPIO 38` (1.5s Fan Test), `GPIO 39` (OLED Page Toggle), `GPIO 1` (Serial Toggle), `GPIO 2` (Reset NVS Counters)।
