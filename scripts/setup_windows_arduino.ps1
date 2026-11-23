$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
$ArduinoDir = Join-Path $env:USERPROFILE "Documents\Arduino"

Write-Host "=== Project 2 (CSE 326) Windows 11 + Arduino Setup ==="

$Sketches = @(
    @{ Name = "p2_hardware_diag"; Files = @("p2_hardware_diag.ino") },
    @{ Name = "p2_acoustic_capture_esp32s3"; Files = @("p2_acoustic_capture_esp32s3.ino") },
    @{ Name = "p2_deployment_trap_esp32s3"; Files = @("p2_deployment_trap_esp32s3.ino", "wingbeat_q1_int8.h") }
)

foreach ($s in $Sketches) {
    $dstDir = Join-Path $ArduinoDir $s.Name
    New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
    foreach ($f in $s.Files) {
        $src = Join-Path $RootDir "firmware\$($s.Name)\$f"
        Copy-Item -Path $src -Destination (Join-Path $dstDir $f) -Force
        Write-Host "[OK] Copied $f -> $dstDir"
    }
}

Write-Host ""
Write-Host "Arduino IDE / CLI Target Settings:"
Write-Host "  - Board Core : esp32 by Espressif Systems v2.0.17"
Write-Host "  - Board      : ESP32S3 Dev Module (FQBN: esp32:esp32:esp32s3)"
Write-Host "  - Right UART Port (CH343, e.g. COM24): USB CDC On Boot = Disabled (CDCOnBoot=default)"
Write-Host "  - Left USB Port  (Native USB-OTG)    : USB CDC On Boot = Enabled  (CDCOnBoot=cdc)"
Write-Host "  - Upload     : 115200 baud | Flash 16MB (128Mb) | OPI PSRAM | 16M Flash (3MB APP/9.9MB FATFS)"
