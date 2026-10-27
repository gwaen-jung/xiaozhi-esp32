#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "config.h"

#define MOTOR_TAG "MotorController"

class MotorController {
private:
    bool initialized_ = false;

    void SetPWM(ledc_channel_t channel, uint32_t duty) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    }

public:
    MotorController() = default;

    void Initialize() {
        if (initialized_) return;

        ledc_timer_config_t timer_conf = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = 20000, // 20kHz inaudible ultrasonic PWM
            .clk_cfg = LEDC_AUTO_CLK
        };
        ledc_timer_config(&timer_conf);

        ledc_channel_config_t ch_conf[4] = {
            { .gpio_num = PIN_MOTOR_A_IN1, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0 },
            { .gpio_num = PIN_MOTOR_A_IN2, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_1, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0 },
            { .gpio_num = PIN_MOTOR_B_IN3, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_2, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0 },
            { .gpio_num = PIN_MOTOR_B_IN4, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_3, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0 }
        };

        for (int i = 0; i < 4; i++) {
            ledc_channel_config(&ch_conf[i]);
        }

        Stop();
        initialized_ = true;
        ESP_LOGI(MOTOR_TAG, "Motor Controller initialized on GPIO 4, 5, 6, 7 (20kHz PWM)");
    }

    void Stop() {
        SetPWM(LEDC_CHANNEL_0, 0);
        SetPWM(LEDC_CHANNEL_1, 0);
        SetPWM(LEDC_CHANNEL_2, 0);
        SetPWM(LEDC_CHANNEL_3, 0);
    }

    void MoveForward(uint8_t speed = 200) {
        SetPWM(LEDC_CHANNEL_0, speed);
        SetPWM(LEDC_CHANNEL_1, 0);
        SetPWM(LEDC_CHANNEL_2, speed);
        SetPWM(LEDC_CHANNEL_3, 0);
    }

    void MoveBackward(uint8_t speed = 200) {
        SetPWM(LEDC_CHANNEL_0, 0);
        SetPWM(LEDC_CHANNEL_1, speed);
        SetPWM(LEDC_CHANNEL_2, 0);
        SetPWM(LEDC_CHANNEL_3, speed);
    }

    void TurnLeft(uint8_t speed = 190) {
        SetPWM(LEDC_CHANNEL_0, 0);
        SetPWM(LEDC_CHANNEL_1, speed);
        SetPWM(LEDC_CHANNEL_2, speed);
        SetPWM(LEDC_CHANNEL_3, 0);
    }

    void TurnRight(uint8_t speed = 190) {
        SetPWM(LEDC_CHANNEL_0, speed);
        SetPWM(LEDC_CHANNEL_1, 0);
        SetPWM(LEDC_CHANNEL_2, 0);
        SetPWM(LEDC_CHANNEL_3, speed);
    }

    void WiggleDance(int repeats = 3) {
        for (int i = 0; i < repeats; i++) {
            TurnLeft(180);
            vTaskDelay(pdMS_TO_TICKS(160));
            TurnRight(180);
            vTaskDelay(pdMS_TO_TICKS(160));
        }
        Stop();
    }
};
