# esp32s3-weact-dog: XIAOZHI desk dog robot

WeAct ESP32-S3 CoreBoard N16R8 + ILI9341 2.4" TFT (animated RoboEyes) + INMP441 mic
+ I2S amp + 4 hobby servos as symbolic legs. Chats through the Xiaozhi cloud.

## Hardware Specification & Verification Status
- **SoC**: ESP32-S3 WeAct Studio CoreBoard (N16R8: 16MB Flash, 8MB Octal PSRAM)
- **LCD Display**: 2.4 inch SPI TFT LCD (Driver: **ILI9341**, 240x320, FSPI port)
- **Audio DAC & Amp**: WeAct Studio **PCM5100A** I2S DAC + Dual Speaker Module
  - *Lưu ý đấu nối*: Chân `MC` (Master Clock) nối xuống **GND** để dùng bộ tạo xung nội PLL. Chân `SD` kéo lên **VCC (3.3V)** hoặc để NC nếu có điện trở pull-up.
- **Microphone**: **INMP441** MEMS I2S Omnidirectional Microphone
- **Servos**: 4x Hobby Micro Servos (9g SG90/MG90S)
  - Chân trên trái (FL): GPIO 7 (180°) | Chân trên phải (FR): GPIO 4 (Servo 360°)
  - Chân dưới trái (RL): GPIO 6 (180°) | Chân dưới phải (RR): GPIO 5 (180°)
  - *Ghi chú phần cứng*: Chân trên phải (GPIO 4) là servo 360° quay liên tục (được điều khiển giữ deadband 90°).
- **Cảm biến tiếp theo (Cập nhật)**:
  - Cảm biến khoảng cách Laser ToF: **VL53L0X** (I2C: SDA GPIO 8, SCL GPIO 9, địa chỉ mặc định `0x29`)
  - Cảm biến định hướng / IMU: **MPU-6050** (I2C: SDA GPIO 8, SCL GPIO 9, địa chỉ mặc định `0x68`)
  - Tích hợp đàm thoại AI Voice Chatbot (Xiaozhi Client)
  - Đã đăng ký MCP Tools: `self.control_dog_legs`, `self.get_front_distance`

## Wiring Table

| Function | ESP32-S3 GPIO | Module pin / Wiring Note |
|---|---|---|
| TFT SCLK | 40 | SCK |
| TFT MOSI | 41 | SDI (MOSI) |
| TFT MISO | 42 (optional, unused) | SDO (MISO) |
| TFT CS | 39 | CS |
| TFT DC | 38 | DC / RS |
| TFT RST | 47 | RESET |
| TFT LED | 3.3 V | LED (Backlight) |
| TFT VCC / GND | 3.3 V / GND | Nguồn màn hình |
| I2S BCLK (shared) | 16 | PCM5100A BCK + INMP441 SCK |
| I2S WS (shared) | 17 | PCM5100A WS + INMP441 WS |
| Mic Data (DIN) | 14 | INMP441 SD |
| Mic L/R, GND, VDD | GND, GND, 3.3 V | L/R nối GND (Left channel) |
| Speaker Data (DOUT) | 15 | PCM5100A DIN |
| PCM5100A MC | GND | Nối GND để bật PLL nội |
| PCM5100A SD | 3.3 V (hoặc NC) | Kéo HIGH bật audio |
| Servo Trên Trái (FL) | 7 | Signal (180°) |
| Servo Trên Phải (FR) | 4 | Signal (Servo 360°) |
| Servo Dưới Trái (RL) | 6 | Signal (180°) |
| Servo Dưới Phải (RR) | 5 | Signal (180°) |
| Servo V+ / GND | 5 V / GND | Chung GND với ESP32 |
| I2C SDA (ToF / MPU6050) | 8 | Cảm biến Laser VL53L0X & MPU6050 |
| I2C SCL (ToF / MPU6050) | 9 | Cảm biến Laser VL53L0X & MPU6050 |
| Push-to-talk | 0 | BOOT button on the board |
| Status LED | 48 | Onboard WS2812 |

## Build (ESP-IDF v6.0.1 or newer)

```sh
# Windows: ESP-IDF PowerShell / CMD from the ESP-IDF installer (v6.0.1+)
# Linux/macOS: source <esp-idf>/export.sh
cd xiaozhi-esp32
python scripts/build.py esp32s3-weact-dog --language vi-VN
idf.py -p COM5 flash monitor        # replace COM5 with your port
```

`--language` picks the voice prompts/UI strings (`en-US`, `zh-CN`, ...).
The first build downloads managed components from components.espressif.com (needs
internet), then takes several minutes.

Bench test of the eyes alone: `idf.py menuconfig` -> enable
"XIAOZHI Dog: cycle eye moods on a debug timer", rebuild. The log prints the fps.

## First start

1. With no saved Wi-Fi the robot opens its own hotspot. The hotspot name and the setup
   page address are shown at the bottom of the screen; connect with a phone, open the
   page and enter your Wi-Fi.
2. After it connects, it speaks a 6-digit code and shows it at the bottom of the screen.
   Add the device with that code on the Xiaozhi console (xiaozhi.me).
3. Click BOOT to start/stop listening. If the wake word is enabled, say it instead.

Press BOOT while it is still starting to re-enter Wi-Fi setup. No credentials are
compiled into the firmware.

## Configuration

| What | Where |
|---|---|
| Pins | `board_pins.h` |
| Panel orientation, colour order, SPI speed, audio rates, default volume | `config.h` |
| Eye colours, sizes, fps; servo trims, directions, range, speed, safety limits | `dog_config.h` |
| State/emotion -> eyes + legs | tables at the top of `dog_behavior.cc` |
| Leg poses and motions | keyframe tables at the top of `leg_servos.cc` |

Volume can also be changed by voice ("turn the volume down"). Things to check on the
real hardware are listed in `docs/HARDWARE_TODO.md`.

## Behaviour

| Trigger | Eyes | Legs |
|---|---|---|
| Boot | open from closed | (servos attach one by one once Wi-Fi + audio are up) then stand |
| Idle | default, blink, drift | stand; sit after 60 s, then PWM off |
| Listening | wide, centred, curious | stand |
| Thinking (your words received) | look up-left | |
| Speaking | keep emotion, centred | soft wiggle every 3.5 s |
| happy / funny / loving / delicious | happy | wiggle |
| laughing | happy + laugh | wave |
| sad / crying | droopy | crouch |
| embarrassed | droopy | |
| angry | angry | |
| surprised / shocked | wide + curious 1.5 s | quick stand |
| confused / thinking | confused shake | |
| sleepy / relaxed | droopy, slow blink | |
| Wi-Fi setup / errors | sweat | |
| anything else | default | |

## Power safety (servos)

Max 2 servos moving at once, start-ups 80 ms apart, 90 deg/s ramps, logical range
45..135 deg, PWM cut after a pose is held 1 s. The reset reason is logged at every
boot; `BROWNOUT` points to the battery/boost converter.

## License

The eye animation is a port of FluxGarage RoboEyes (GPL-3.0-or-later,
`LICENSE-RoboEyes.txt`), so a firmware binary containing this board is GPL-3.0.
The subtitle font bitmaps are generated from DejaVu Sans Mono (`tools/gen_font_8x16.py`).
