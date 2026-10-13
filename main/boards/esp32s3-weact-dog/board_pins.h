// Single source of truth for every GPIO used (or reserved) by the
// XIAOZHI dog robot on a WeAct ESP32-S3 CoreBoard N16R8.
#pragma once

#include <driver/gpio.h>

// ---- TFT ILI9341 2.4" (SPI) ------------------------------------------------
#define PIN_TFT_DC    GPIO_NUM_38
#define PIN_TFT_CS    GPIO_NUM_39
#define PIN_TFT_SCLK  GPIO_NUM_40
#define PIN_TFT_MOSI  GPIO_NUM_41
#define PIN_TFT_MISO  GPIO_NUM_NC  // GPIO42 is wired but unused (write-only panel)
#define PIN_TFT_RST   GPIO_NUM_47
// TODO(hw-verify): backlight is tied to 3.3V. Set a GPIO here to get PWM dimming.
#define PIN_TFT_BL    GPIO_NUM_NC

// ---- Microphone INMP441 + WeAct PCM5100A Amp (Full-Duplex I2S) ------------
#define PIN_I2S_BCLK  GPIO_NUM_16  // shared BCK (PCM5100A BCK & INMP441 SCK)
#define PIN_I2S_WS    GPIO_NUM_17  // shared WS / LRC (PCM5100A WS & INMP441 WS)
#define PIN_I2S_DIN   GPIO_NUM_14  // INMP441 SD (mic data in)
#define PIN_I2S_DOUT  GPIO_NUM_15  // PCM5100A DIN (speaker data out)
// SD pin on PCM5100A amp module (tied to VCC = NC; set GPIO if wired)
#define PIN_AMP_SD    GPIO_NUM_NC

// Backward-compat aliases
#define PIN_MIC_WS    PIN_I2S_WS
#define PIN_MIC_SCK   PIN_I2S_BCLK
#define PIN_MIC_SD    PIN_I2S_DIN
#define PIN_AMP_BCLK  PIN_I2S_BCLK
#define PIN_AMP_LRC   PIN_I2S_WS
#define PIN_AMP_DIN   PIN_I2S_DOUT

// ---- Servo legs (Front: FL=7, FR=4 (360) | Rear: RL=6, RR=5) ----------------
#define PIN_SERVO_FL  GPIO_NUM_7  // Trên Trái (180°)
#define PIN_SERVO_FR  GPIO_NUM_4  // Trên Phải (Servo 360°)
#define PIN_SERVO_RL  GPIO_NUM_6  // Dưới Trái (180°)
#define PIN_SERVO_RR  GPIO_NUM_5  // Dưới Phải (180°)

// ---- I2C Sensors (VL53L0X ToF Laser + MPU6050 IMU) -------------------------
#define PIN_I2C_SDA     GPIO_NUM_8
#define PIN_I2C_SCL     GPIO_NUM_9

// ---- UI --------------------------------------------------------------------
#define PIN_BUTTON_TALK GPIO_NUM_0   // BOOT button, input only
#define PIN_STATUS_LED  GPIO_NUM_48  // onboard WS2812

// ---- Reserved for later (do not use) ---------------------------------------
// USB D- 19 / D+ 20      : camera / native USB
// Never use: 0 as output, 3/45/46 (strapping), 26..32 (flash), 35..37 (OPI PSRAM)
