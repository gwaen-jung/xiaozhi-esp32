param(
    [string]$Port = ""
)

$ErrorActionPreference = "Stop"

$python = "C:\Users\trung\.platformio\penv\Scripts\python.exe"

if (-not $Port) {
    $ports = & $python -c "import serial.tools.list_ports as lp; print(';'.join([p.device for p in lp.comports()]))"
    if ($ports) {
        $Port = $ports.Split(';')[0]
        Write-Host "-> Tu dong chon cong: $Port" -ForegroundColor Green
    } else {
        $Port = "COM7"
    }
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Flashing Xiaozhi Companion Robot Firmware to $Port"       -ForegroundColor Green
Write-Host "  - Wake Word: 'Chào Nhi' (Siri-style voice wake)"          -ForegroundColor Yellow
Write-Host "  - Display: OLED SSD1306 128x64"                           -ForegroundColor Yellow
Write-Host "  - Audio: INMP441 + MAX98357A (GPIO 11, 12, 13, 14)"       -ForegroundColor Yellow
Write-Host "  - Motors: L298N DC (GPIO 4, 5, 6, 7)"                     -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan

Set-Location "$PSScriptRoot\build"

& $python -m esptool --chip esp32s3 -p $Port -b 921600 --before default-reset --after hard-reset write-flash `
    --flash-mode dio --flash-size 16MB --flash-freq 80m `
    0x0 "bootloader\bootloader.bin" `
    0x8000 "partition_table\partition-table.bin" `
    0xd000 "ota_data_initial.bin" `
    0x20000 "xiaozhi.bin" `
    0x800000 "generated_assets.bin"

Set-Location $PSScriptRoot

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n==> NAP FIRMWARE XIAOZHI THANH CONG 100%!" -ForegroundColor Green
} else {
    Write-Host "`n==> Nap firmware that bai! Hay giu nut BOOT va bam nhan RST roi thu lai." -ForegroundColor Red
}
