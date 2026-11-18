@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo    AI Robot ESP32-S3 One-Click Flash Tool (CMD)
echo    Target: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)
echo =======================================================

set PORT=%1
if "%PORT%"=="" (
    set /p PORT="Enter your ESP32-S3 COM Port (e.g. COM5): "
)

if "%PORT%"=="" (
    echo [ERROR] No COM port provided.
    exit /b 1
)

set BOOTLOADER=.pio\build\esp32-s3-n16r8\bootloader.bin
set PARTITIONS=.pio\build\esp32-s3-n16r8\partitions.bin
set BOOTAPP0=boot_app0.bin
set FIRMWARE=.pio\build\esp32-s3-n16r8\firmware.bin
set SRMODELS=srmodels.bin

if not exist "%BOOTAPP0%" (
    if exist "%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" (
        copy "%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" "%BOOTAPP0%" >nul
    )
)

if not exist "%BOOTLOADER%" (
    echo [ERROR] Missing %BOOTLOADER%. Build firmware first.
    exit /b 1
)
if not exist "%PARTITIONS%" (
    echo [ERROR] Missing %PARTITIONS%.
    exit /b 1
)
if not exist "%BOOTAPP0%" (
    echo [ERROR] Missing %BOOTAPP0%.
    exit /b 1
)
if not exist "%FIRMWARE%" (
    echo [ERROR] Missing %FIRMWARE%.
    exit /b 1
)
if not exist "%SRMODELS%" (
    echo [ERROR] Missing %SRMODELS%.
    exit /b 1
)

set ESPTOOL_EXE="%USERPROFILE%\.platformio\packages\tool-esptool\esptool.exe"
if not exist %ESPTOOL_EXE% (
    set ESPTOOL_EXE=python -m esptool
)

echo [OK] All binaries verified. Flashing to %PORT% at 921600 baud...
echo [INFO] Bootloader at 0x0000, Partitions at 0x8000, BootApp0 at 0xE000, App0 at 0x10000, Model at 0xC10000
echo [INFO] NVS at 0x9000 is preserved.

%ESPTOOL_EXE% --chip esp32s3 --port %PORT% --baud 921600 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0000 "%BOOTLOADER%" 0x8000 "%PARTITIONS%" 0xe000 "%BOOTAPP0%" 0x10000 "%FIRMWARE%" 0xc10000 "%SRMODELS%"

if %ERRORLEVEL% equ 0 (
    echo.
    echo =======================================================
    echo   [SUCCESS] Full System, OTA Reset ^& WakeNet9 Flashed!
    echo =======================================================
) else (
    echo.
    echo [ERROR] Flashing failed with error code %ERRORLEVEL%.
)
pause
