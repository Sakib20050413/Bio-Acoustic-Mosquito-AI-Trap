#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== Project 2 (CSE 326) Ubuntu + Arduino IDE Auto-Setup ==="
mkdir -p "$HOME/Arduino/p2_hardware_diag" \
         "$HOME/Arduino/p2_acoustic_capture_esp32s3" \
         "$HOME/Arduino/p2_deployment_trap_esp32s3"

cp -v "$ROOT_DIR/firmware/p2_hardware_diag/p2_hardware_diag.ino" "$HOME/Arduino/p2_hardware_diag/"
cp -v "$ROOT_DIR/firmware/p2_acoustic_capture_esp32s3/p2_acoustic_capture_esp32s3.ino" "$HOME/Arduino/p2_acoustic_capture_esp32s3/"
cp -v "$ROOT_DIR/firmware/p2_deployment_trap_esp32s3/p2_deployment_trap_esp32s3.ino" "$HOME/Arduino/p2_deployment_trap_esp32s3/"
cp -v "$ROOT_DIR/firmware/p2_deployment_trap_esp32s3/wingbeat_q1_int8.h" "$HOME/Arduino/p2_deployment_trap_esp32s3/"

echo ""
echo "[OK] All 3 Arduino sketch folders copied to $HOME/Arduino/"
echo "Reminder — Arduino IDE 1.8.19 Tools Settings:"
echo "  - Board: ESP32S3 Dev Module (esp32 by Espressif Systems v2.0.17)"
echo "  - Right UART Port (CH343): USB CDC On Boot = Disabled"
echo "  - Left Native USB Port   : USB CDC On Boot = Enabled"
echo "  - Upload Speed: 115200"
echo "  - Flash Size: 16MB (128Mb) | PSRAM: OPI PSRAM"
echo "  - Partition: 16M Flash (3MB APP/9.9MB FATFS)"
echo ""
echo "Tip: If /dev/ttyACM0 upload hangs on Ubuntu, run: sudo systemctl stop ModemManager"
