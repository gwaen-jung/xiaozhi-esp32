param(
    [string]$Port = "COM18"
)

$ErrorActionPreference = "Continue"

$env:IDF_PATH = "C:\Users\trung\.platformio\packages\framework-espidf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\trung\.platformio\penv"
$env:ESP_IDF_VERSION = "6.0.1"
$env:PATH = "C:\Users\trung\.platformio\penv\Scripts;C:\Users\trung\.platformio\packages\toolchain-xtensa-esp-elf\bin;C:\Users\trung\.platformio\packages\tool-cmake\bin;C:\Users\trung\.platformio\packages\framework-espidf\tools;$env:PATH"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Flashing Xiaozhi Robot Dog Firmware to $Port" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

python -c "
import serial, time
s = serial.Serial('$Port', 115200, timeout=1)
s.dtr = False
s.rts = True
time.sleep(0.1)
s.dtr = True
s.rts = False
time.sleep(0.1)
s.dtr = False
time.sleep(0.2)
s.close()
"

python -m esptool --chip esp32s3 -p $Port -b 460800 --before no-reset --after hard-reset write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x8000 build\partition_table\partition-table.bin 0xd000 build\ota_data_initial.bin 0x20000 build\xiaozhi.bin
