param(
    [switch]$InstallOnly
)
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir

function Find-ArduinoCli {
    $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in @(
        "C:\Program Files\Arduino CLI\arduino-cli.exe",
        "$env:LOCALAPPDATA\Programs\Arduino CLI\arduino-cli.exe"
    )) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  PROJECT 2 (CSE 326 WingBeat-TinyNet) HOME PC AUTO-SETUP " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Install arduino-cli if missing
$acli = Find-ArduinoCli
if (-not $acli) {
    Write-Host "[1/5] Installing Arduino CLI via winget..." -ForegroundColor Yellow
    winget install --id ArduinoSA.CLI --exact --accept-source-agreements --accept-package-agreements --silent
    $acli = Find-ArduinoCli
    if (-not $acli) { throw "arduino-cli install failed or PATH needs terminal restart." }
} else {
    Write-Host "[1/5] Found arduino-cli: $acli" -ForegroundColor Green
}

# 2. Configure arduino-cli + Espressif v2.0.17 + 4 Libraries
Write-Host "[2/5] Configuring ESP32 v2.0.17 core & libraries..." -ForegroundColor Yellow
$ArduinoUserDir = Join-Path $env:USERPROFILE "Documents\Arduino"
New-Item -ItemType Directory -Force -Path $ArduinoUserDir | Out-Null

& $acli config init --overwrite | Out-Null
& $acli config set directories.user $ArduinoUserDir
& $acli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

$coreList = & $acli core list 2>$null | Out-String
if ($coreList -notmatch "esp32:esp32\s+2\.0\.17") {
    & $acli core update-index
    & $acli core install esp32:esp32@2.0.17
} else {
    Write-Host "      esp32:esp32@2.0.17 already installed." -ForegroundColor Green
}

& $acli lib install "arduinoFFT" "DHT sensor library" "Adafruit Unified Sensor" "U8g2"

# 3. Copy all 3 Arduino sketches to Documents\Arduino
Write-Host "[3/5] Syncing sketches to $ArduinoUserDir..." -ForegroundColor Yellow
& powershell -ExecutionPolicy Bypass -File (Join-Path $ScriptDir "setup_windows_arduino.ps1")

# 4. Setup Python ML .venv
Write-Host "[4/5] Checking Python ML .venv..." -ForegroundColor Yellow
$VenvPy = Join-Path $RootDir ".venv\Scripts\python.exe"
if (-not (Test-Path $VenvPy)) {
    python -m venv (Join-Path $RootDir ".venv")
}
& (Join-Path $RootDir ".venv\Scripts\pip.exe") install -q -r (Join-Path $RootDir "ml_pipeline\requirements.txt")
Write-Host "      Python .venv ready!" -ForegroundColor Green

if ($InstallOnly) {
    Write-Host "[DONE] Setup complete! Run again without -InstallOnly for flash menu." -ForegroundColor Green
    exit 0
}

# 5. Interactive Menu
while ($true) {
    Write-Host ""
    Write-Host "=== HOME PC QUICK ACTION MENU ===" -ForegroundColor Cyan
    Write-Host "  [1] Flash DIAG Sketch      (p2_hardware_diag -> UART/COM)"
    Write-Host "  [2] Flash CAPTURE Sketch   (p2_acoustic_capture_esp32s3 -> UART/COM)"
    Write-Host "  [3] Flash FINAL TRAP       (p2_deployment_trap_esp32s3 -> UART/COM)"
    Write-Host "  [4] Open Serial Monitor    (115200 baud)"
    Write-Host "  [5] Start Phone Cam Bridge (port 8765)"
    Write-Host "  [6] Run ML Smoke Test"
    Write-Host "  [0] Exit"
    $choice = Read-Host "Select option"
    if ($choice -eq "0") { break }

    if ($choice -in @("1","2","3","4")) {
        $ports = Get-CimInstance Win32_SerialPort | Where-Object { $_.DeviceID -ne "COM1" }
        $ports | Select-Object DeviceID, Description | Format-Table -AutoSize
        $defaultPort = ($ports | Select-Object -First 1).DeviceID
        $port = Read-Host "Enter COM port (default: $defaultPort)"
        if ([string]::IsNullOrWhiteSpace($port)) { $port = $defaultPort }
        $usbType = Read-Host "Port plugged into [U]ART (Right, default) or [N]ative USB (Left)? (U/N)"
        $cdc = if ($usbType -match "^[Nn]") { "cdc" } else { "default" }
        $fqbn = "esp32:esp32:esp32s3:CDCOnBoot=$cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=115200"
    }

    switch ($choice) {
        "1" {
            & $acli compile --fqbn $fqbn "$ArduinoUserDir\p2_hardware_diag"
            & $acli upload -p $port --fqbn $fqbn "$ArduinoUserDir\p2_hardware_diag"
            & powershell -ExecutionPolicy Bypass -File (Join-Path $ScriptDir "read_diag.ps1") -Port $port -Seconds 6
        }
        "2" {
            & $acli compile --fqbn $fqbn "$ArduinoUserDir\p2_acoustic_capture_esp32s3"
            & $acli upload -p $port --fqbn $fqbn "$ArduinoUserDir\p2_acoustic_capture_esp32s3"
            & powershell -ExecutionPolicy Bypass -File (Join-Path $ScriptDir "read_diag.ps1") -Port $port -Seconds 6
        }
        "3" {
            & $acli compile --fqbn $fqbn "$ArduinoUserDir\p2_deployment_trap_esp32s3"
            & $acli upload -p $port --fqbn $fqbn "$ArduinoUserDir\p2_deployment_trap_esp32s3"
            & powershell -ExecutionPolicy Bypass -File (Join-Path $ScriptDir "read_diag.ps1") -Port $port -Seconds 6
        }
        "4" {
            & powershell -ExecutionPolicy Bypass -File (Join-Path $ScriptDir "read_diag.ps1") -Port $port -Seconds 15
        }
        "5" {
            & $VenvPy -u (Join-Path $ScriptDir "phone_cam_bridge.py")
        }
        "6" {
            & $VenvPy (Join-Path $RootDir "ml_pipeline\p2_humbugdb_q1_training_and_snr_eval.py") --smoke-test --out (Join-Path $RootDir "ml_pipeline\smoke_test_out")
        }
    }
}
