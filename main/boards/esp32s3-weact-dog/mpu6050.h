// MPU6050 6-Axis Motion Tracking Sensor Driver for ESP-IDF v6 (i2c_master)
#pragma once

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <cstdint>

class MPU6050 {
public:
    static constexpr uint8_t kDefaultAddress = 0x68;

    MPU6050(i2c_master_bus_handle_t bus_handle, uint8_t address = kDefaultAddress)
        : bus_handle_(bus_handle), address_(address) {}

    bool Init() {
        // Probe specifically for MPU6050 at 0x68 or 0x69 (AD0 high)
        ESP_LOGI(kTag, "Probing MPU6050 at 0x68...");
        if (i2c_master_probe(bus_handle_, 0x68, 50) == ESP_OK) {
            address_ = 0x68;
            ESP_LOGI(kTag, "Found MPU6050 at 0x68!");
        } else if (i2c_master_probe(bus_handle_, 0x69, 50) == ESP_OK) {
            address_ = 0x69;
            ESP_LOGI(kTag, "Found MPU6050 at 0x69 (AD0 pin HIGH)!");
        } else {
            ESP_LOGW(kTag, "No MPU6050 responding at 0x68 or 0x69");
            return false;
        }

        if (dev_handle_ == nullptr) {
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = address_,
                .scl_speed_hz = 100000,
                .scl_wait_us = 0,
                .flags = { .disable_ack_check = 0 },
            };
            esp_err_t err = i2c_master_bus_add_device(bus_handle_, &dev_cfg, &dev_handle_);
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "Failed to add I2C device: %s", esp_err_to_name(err));
                return false;
            }
        }

        // Check WHO_AM_I register (0x75, should be 0x68)
        uint8_t who_am_i = 0;
        if (!ReadReg(0x75, &who_am_i)) {
            ESP_LOGW(kTag, "Failed to read MPU6050 WHO_AM_I register");
            return false;
        }
        ESP_LOGI(kTag, "MPU6050 WHO_AM_I = 0x%02X", who_am_i);
        if (who_am_i != 0x68 && who_am_i != 0x70 && who_am_i != 0x72) {
            ESP_LOGW(kTag, "Unexpected WHO_AM_I: 0x%02X (expected 0x68)", who_am_i);
            return false;
        }

        // Wake up MPU6050 (write 0x00 to PWR_MGMT_1, reg 0x6B)
        WriteReg(0x6B, 0x00);
        vTaskDelay(pdMS_TO_TICKS(10));

        // Set sample rate divider = 7 (1kHz / 8 = 125Hz)
        WriteReg(0x19, 0x07);
        // DLPF config = 3 (approx 44Hz filter)
        WriteReg(0x1A, 0x03);
        // Accel full scale = +/- 2g (reg 0x1C = 0x00)
        WriteReg(0x1C, 0x00);
        // Gyro full scale = +/- 250 deg/s (reg 0x1B = 0x00)
        WriteReg(0x1B, 0x00);

        initialized_ = true;
        ESP_LOGI(kTag, "MPU6050 initialized successfully!");
        return true;
    }

    struct MotionData {
        float ax_g = 0;
        float ay_g = 0;
        float az_g = 0;
        float pitch_deg = 0;
        float roll_deg = 0;
        bool is_picked_up = false;
    };

    bool ReadMotion(MotionData* out) {
        if (!initialized_) return false;

        uint8_t buf[14] = {0};
        uint8_t reg = 0x3B;
        if (i2c_master_transmit_receive(dev_handle_, &reg, 1, buf, 14, 50) != ESP_OK) {
            return false;
        }

        int16_t raw_ax = (buf[0] << 8) | buf[1];
        int16_t raw_ay = (buf[2] << 8) | buf[3];
        int16_t raw_az = (buf[4] << 8) | buf[5];

        out->ax_g = raw_ax / 16384.0f;
        out->ay_g = raw_ay / 16384.0f;
        out->az_g = raw_az / 16384.0f;

        // Calculate pitch and roll angles
        out->pitch_deg = atan2f(out->ay_g, sqrtf(out->ax_g * out->ax_g + out->az_g * out->az_g)) * 180.0f / M_PI;
        out->roll_deg = atan2f(-out->ax_g, out->az_g) * 180.0f / M_PI;

        // Total acceleration magnitude
        float total_g = sqrtf(out->ax_g * out->ax_g + out->ay_g * out->ay_g + out->az_g * out->az_g);
        // Ignore read glitches / zero-byte errors
        if (total_g < 0.4f || total_g > 3.0f) {
            out->is_picked_up = false;
            last_data_ = *out;
            return true;
        }

        // MPU6050 is mounted inverted on robot chassis, so normal resting roll is ~ ±180 deg.
        float pitch_dev = fabsf(out->pitch_deg);
        float roll_dev = fabsf(fabsf(out->roll_deg) - 180.0f);
        // Only trigger pickup if the robot body is truly tilted sideways or lifted (> 45 deg tilt)
        out->is_picked_up = (pitch_dev > 45.0f || roll_dev > 45.0f);

        last_data_ = *out;
        return true;
    }

    const MotionData& GetLastMotion() const { return last_data_; }
    bool IsInitialized() const { return initialized_; }

private:
    static constexpr const char* kTag = "MPU6050";

    bool WriteReg(uint8_t reg, uint8_t val) {
        uint8_t buf[2] = {reg, val};
        return i2c_master_transmit(dev_handle_, buf, 2, 50) == ESP_OK;
    }

    bool ReadReg(uint8_t reg, uint8_t* val) {
        return i2c_master_transmit_receive(dev_handle_, &reg, 1, val, 1, 50) == ESP_OK;
    }

    i2c_master_bus_handle_t bus_handle_ = nullptr;
    i2c_master_dev_handle_t dev_handle_ = nullptr;
    uint8_t address_;
    bool initialized_ = false;
    MotionData last_data_;
};
