<#
.SYNOPSIS
  1-Click Deploy WingBeat-TinyNet v5.1 Real Mosquito Audio Hub (web_demo/) to Vercel Production!
.DESCRIPTION
  Runs `npx vercel --prod` inside `web_demo/` (zero build step, 52 lossless 44.1 kHz WAVs + 3D CAD + 44-Pin Wiring).
#>
$ErrorActionPreference = "Stop"
$WebDir = Join-Path (Split-Path -Parent $PSScriptRoot) "web_demo"
if (-not (Test-Path "$WebDir\index.html")) {
    Write-Error "web_demo\index.html not found at $WebDir"
    exit 1
}
Write-Host "=== Deploying WingBeat-TinyNet v5.1 Audio Hub (web_demo/) to Vercel Production ===" -ForegroundColor Cyan
Set-Location $WebDir
npx --yes vercel --prod
