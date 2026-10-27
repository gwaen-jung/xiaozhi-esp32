$ErrorActionPreference = "Stop"

$env:IDF_PATH = "C:\Users\trung\.platformio\packages\framework-espidf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\trung\.platformio\penv"
$env:ESP_IDF_VERSION = "6.0.1"
$env:PYTHONIOENCODING = "utf-8"
$env:PATH = "C:\Users\trung\.platformio\penv\Scripts;C:\Users\trung\.platformio\packages\toolchain-xtensa-esp-elf\bin;C:\Users\trung\.platformio\packages\tool-cmake\bin;C:\Users\trung\.platformio\packages\framework-espidf\tools;$env:PATH"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Building Xiaozhi Companion Robot Firmware (esp32s3-companion-robot)" -ForegroundColor Green
Write-Host "  - OLED SSD1306 (GPIO 1, 2)" -ForegroundColor Green
Write-Host "  - Audio I2S INMP441 + MAX98357A (GPIO 11, 12, 13, 14)" -ForegroundColor Green
Write-Host "  - Motor L298N (GPIO 4, 5, 6, 7)" -ForegroundColor Green
Write-Host "  - Language: Vietnamese (vi-VN)" -ForegroundColor Yellow
Write-Host "  - Siri-Style Voice Wake: Chào Nhi" -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan

& "C:\Users\trung\.platformio\penv\Scripts\python.exe" scripts/build.py esp32s3-companion-robot --language vi-VN $args
