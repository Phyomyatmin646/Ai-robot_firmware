<#
.SYNOPSIS
    ESP32-S3 Smart Family AI Robot — One-Click Complete Flash Script
.DESCRIPTION
    Flashes Bootloader, Partition Table, Boot App0 (OTA Data Initializer), Robot Firmware, and ESP-SR WakeNet9 Model in a single unified command.
    Preserves NVS partition (0x9000 - 0xDFFF). Uses esptool v5 hyphenated syntax.
.EXAMPLE
    .\flash_all.ps1 -Port COM5
    .\flash_all.ps1
#>

param(
    [string]$Port = ""
)

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "   AI Robot ESP32-S3 One-Click Flash Tool (OTA Safe)" -ForegroundColor Cyan
Write-Host "   Target: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

# 1. Resolve Binary Paths
$FirmwareDir = $PSScriptRoot
$Bootloader = "$FirmwareDir\.pio\build\esp32-s3-n16r8\bootloader.bin"
$Partitions = "$FirmwareDir\.pio\build\esp32-s3-n16r8\partitions.bin"
$BootApp0   = "$FirmwareDir\boot_app0.bin"
$Firmware   = "$FirmwareDir\.pio\build\esp32-s3-n16r8\firmware.bin"
$SrModels   = "$FirmwareDir\srmodels.bin"

# If boot_app0.bin is missing in workspace root, copy from platformio packages
if (-not (Test-Path $BootApp0)) {
    $pioBootApp0 = "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
    if (Test-Path $pioBootApp0) {
        Copy-Item $pioBootApp0 $BootApp0 -Force
    }
}

$AllExist = $true
foreach ($file in @($Bootloader, $Partitions, $BootApp0, $Firmware, $SrModels)) {
    if (-not (Test-Path $file)) {
        Write-Host "[ERROR] Missing required file: $file" -ForegroundColor Red
        $AllExist = $false
    }
}

if (-not $AllExist) {
    Write-Host "[ERROR] Please build the project with PlatformIO first before flashing." -ForegroundColor Red
    exit 1
}

Write-Host "[OK] All required binaries verified:" -ForegroundColor Green
Write-Host "  - 0x000000 : Bootloader   ($(Get-Item $Bootloader | Select-Object -ExpandProperty Length) bytes)"
Write-Host "  - 0x008000 : Partitions   ($(Get-Item $Partitions | Select-Object -ExpandProperty Length) bytes)"
Write-Host "  - [PRESERVED: NVS 0x009000 - 0x00DFFF (Wi-Fi & Device Credentials Untouched)]" -ForegroundColor DarkGray
Write-Host "  - 0x00e000 : Boot App0    ($(Get-Item $BootApp0 | Select-Object -ExpandProperty Length) bytes -> Resets OTA to APP0)"
Write-Host "  - 0x010000 : Firmware App ($(Get-Item $Firmware | Select-Object -ExpandProperty Length) bytes)"
Write-Host "  - 0x0c10000: WakeNet9     ($(Get-Item $SrModels | Select-Object -ExpandProperty Length) bytes)"

# 2. Detect COM Port if not supplied
if ([string]::IsNullOrWhiteSpace($Port)) {
    $ports = Get-CimInstance -ClassName Win32_SerialPort | Select-Object -ExpandProperty DeviceID
    if ($ports) {
        if ($ports.Count -eq 1) {
            $Port = $ports
            Write-Host "[INFO] Auto-detected COM Port: $Port" -ForegroundColor Yellow
        } else {
            Write-Host "[INFO] Multiple COM ports detected: $($ports -join ', ')" -ForegroundColor Yellow
            $Port = Read-Host "Please enter your ESP32-S3 COM Port (e.g. COM5)"
        }
    } else {
        $Port = Read-Host "Please enter your ESP32-S3 COM Port (e.g. COM5)"
    }
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    Write-Host "[ERROR] No COM port specified. Aborting." -ForegroundColor Red
    exit 1
}

# 3. Locate Esptool Executable & Execute Unified Flash Command
$pioEsptool = "$env:USERPROFILE\.platformio\packages\tool-esptool\esptool.exe"

Write-Host "`n[START] Flashing full system to ESP32-S3 on $Port at 921600 baud..." -ForegroundColor Cyan

if (Test-Path $pioEsptool) {
    & $pioEsptool --chip esp32s3 --port $Port --baud 921600 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0000 $Bootloader 0x8000 $Partitions 0xe000 $BootApp0 0x10000 $Firmware 0xc10000 $SrModels
} else {
    python -m esptool --chip esp32s3 --port $Port --baud 921600 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0000 $Bootloader 0x8000 $Partitions 0xe000 $BootApp0 0x10000 $Firmware 0xc10000 $SrModels
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=======================================================" -ForegroundColor Green
    Write-Host "   [SUCCESS] Full System & WakeNet9 Flashed Successfully!" -ForegroundColor Green
    Write-Host "   Boot Slot: Explicitly initialized to APP0 (0x10000)" -ForegroundColor Green
    Write-Host "   NVS Data : Preserved at 0x9000 (Credentials safe)" -ForegroundColor Green
    Write-Host "=======================================================" -ForegroundColor Green
    Write-Host "You can now open Serial Monitor at 115200 baud to interact with your robot."
} else {
    Write-Host "`n[ERROR] Flashing failed with exit code $LASTEXITCODE." -ForegroundColor Red
}
