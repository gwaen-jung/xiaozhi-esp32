# esp32s3-weact-dog: XIAOZHI desk dog robot

WeAct ESP32-S3 CoreBoard N16R8 + ILI9341 2.4" TFT (animated RoboEyes) + INMP441 mic
+ I2S amp + 4 hobby servos as symbolic legs. Chats through the Xiaozhi cloud.

## Wiring

| Function | ESP32-S3 GPIO | Module pin |
|---|---|---|
| TFT SCLK | 40 | SCK |
| TFT MOSI | 41 | SDI (MOSI) |
| TFT MISO | 42 (optional, unused) | SDO (MISO) |
| TFT CS | 39 | CS |
| TFT DC | 38 | DC / RS |
| TFT RST | 47 | RESET |
| TFT LED | 3.3 V | LED |
| TFT VCC / GND | 3.3 V / GND | |
| Mic WS | 4 | INMP441 WS |
| Mic SCK | 5 | INMP441 SCK |
| Mic SD | 6 | INMP441 SD |
| Mic L/R, GND, VDD | GND, GND, 3.3 V | |
| Amp BCLK | 15 | BCLK |
| Amp LRC | 16 | LRC / WS |
| Amp DIN | 17 | DIN |
| Amp SD (enable) | 18 | SD / EN |
| Amp VIN | 5 V (boost) | |
| Servo front-left | 9 | signal |
| Servo front-right | 10 | signal |
| Servo rear-left | 11 | signal |
| Servo rear-right | 12 | signal |
| Servo V+ / GND | 5 V (boost) / GND (common with ESP32) | |
| Push-to-talk | 0 | BOOT button on the board |
| Status LED | 48 | onboard WS2812 |
| Reserved | 7 SDA / 8 SCL (ToF, IMU), 19/20 (USB) | not used yet |

All pins are defined only in `board_pins.h`.

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
