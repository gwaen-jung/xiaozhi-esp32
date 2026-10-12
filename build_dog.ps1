$ErrorActionPreference = "Stop"

$env:IDF_PATH = "C:\Users\trung\.platformio\packages\framework-espidf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\trung\.platformio\penv"
$env:ESP_IDF_VERSION = "6.0.1"
$env:PATH = "C:\Users\trung\.platformio\penv\Scripts;C:\Users\trung\.platformio\packages\toolchain-xtensa-esp-elf\bin;C:\Users\trung\.platformio\packages\tool-cmake\bin;C:\Users\trung\.platformio\packages\framework-espidf\tools;$env:PATH"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Building Xiaozhi Robot Dog Firmware (esp32s3-weact-dog)" -ForegroundColor Green
Write-Host "  Language: Vietnamese (vi-VN)" -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan

python scripts/build.py esp32s3-weact-dog --language vi-VN $args
