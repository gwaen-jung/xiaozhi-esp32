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

// ---- Microphone INMP441 (I2S RX, L/R -> GND = left slot) -------------------
#define PIN_MIC_WS    GPIO_NUM_4
#define PIN_MIC_SCK   GPIO_NUM_5
#define PIN_MIC_SD    GPIO_NUM_6

// ---- Amplifier, WeAct I2S dual-speaker amp (I2S TX) ------------------------
// TODO(hw-verify): pinout assumed MAX98357A-style (BCLK, LRC, DIN, SD).
#define PIN_AMP_BCLK  GPIO_NUM_15
#define PIN_AMP_LRC   GPIO_NUM_16
#define PIN_AMP_DIN   GPIO_NUM_17
// TODO(hw-verify): SD/enable pin. High = enabled on MAX98357A. NC to disable.
#define PIN_AMP_SD    GPIO_NUM_18

// ---- Servo legs ------------------------------------------------------------
#define PIN_SERVO_FL  GPIO_NUM_9
#define PIN_SERVO_FR  GPIO_NUM_10
#define PIN_SERVO_RL  GPIO_NUM_11
#define PIN_SERVO_RR  GPIO_NUM_12

// ---- UI --------------------------------------------------------------------
#define PIN_BUTTON_TALK GPIO_NUM_0   // BOOT button, input only
#define PIN_STATUS_LED  GPIO_NUM_48  // onboard WS2812

// ---- Reserved for later (do not use) ---------------------------------------
// I2C SDA 7 / SCL 8      : VL53L0X ToF, MPU6050
// USB D- 19 / D+ 20      : camera / native USB
// Never use: 0 as output, 3/45/46 (strapping), 26..32 (flash), 35..37 (OPI PSRAM)
