// Universal Distance Sensor Driver: supports both VL53L0X and VL53L1X on ESP-IDF v6
#pragma once

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>

enum class TofModel : uint8_t {
    kUnknown,
    kVL53L0X,
    kVL53L1X,
};

class VL53L0X {
public:
    static constexpr uint8_t kDefaultAddress = 0x29;

    VL53L0X(i2c_master_bus_handle_t bus_handle, uint8_t address = kDefaultAddress)
        : bus_handle_(bus_handle), address_(address) {}

    bool Init() {
        vTaskDelay(pdMS_TO_TICKS(50));

        // Scan I2C bus to find responding device address
        ESP_LOGI(kTag, "Scanning I2C bus...");
        for (uint8_t a = 0x08; a < 0x78; ++a) {
            if (i2c_master_probe(bus_handle_, a, pdMS_TO_TICKS(5)) == ESP_OK) {
                ESP_LOGI(kTag, "-> Found I2C device at 0x%02X", a);
                if (a == 0x29 || a == 0x28) {
                    address_ = a;
                }
            }
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

        // Try detecting VL53L1X first (16-bit register address 0x010F = 0xEA)
        uint16_t l1_id = 0;
        if (ReadReg16BitAddr16(0x010F, &l1_id)) {
            ESP_LOGI(kTag, "Checked reg16(0x010F) = 0x%04X", l1_id);
            if (l1_id == 0xEACC || (l1_id >> 8) == 0xEA) {
                model_ = TofModel::kVL53L1X;
                ESP_LOGI(kTag, "Detected sensor: VL53L1X!");
                return InitVL53L1X();
            }
        }

        // Try detecting VL53L0X (8-bit register address 0xC0 = 0xEE)
        uint8_t l0_id = 0;
        if (ReadReg8(0xC0, &l0_id)) {
            ESP_LOGI(kTag, "Checked reg8(0xC0) = 0x%02X", l0_id);
            if (l0_id == 0xEE) {
                model_ = TofModel::kVL53L0X;
                ESP_LOGI(kTag, "Detected sensor: VL53L0X!");
                return InitVL53L0X();
            }
        }

        ESP_LOGW(kTag, "Neither VL53L0X nor VL53L1X detected at 0x%02X", address_);
        return false;
    }

    uint16_t ReadDistanceMm() {
        if (!initialized_) return 0;
        if (model_ == TofModel::kVL53L1X) {
            return ReadDistanceL1X();
        } else {
            return ReadDistanceL0X();
        }
    }

    uint16_t GetLastDistanceMm() const { return last_distance_mm_; }
    bool IsInitialized() const { return initialized_; }
    TofModel GetModel() const { return model_; }

private:
    static constexpr const char* kTag = "ToF_Sensor";

    // ------------------------------------------------------------------------
    // Low-level I2C helpers for 8-bit register addresses (VL53L0X)
    // ------------------------------------------------------------------------
    bool WriteReg8(uint8_t reg, uint8_t val) {
        uint8_t buf[2] = {reg, val};
        return i2c_master_transmit(dev_handle_, buf, 2, pdMS_TO_TICKS(50)) == ESP_OK;
    }

    bool WriteReg8_16(uint8_t reg, uint16_t val) {
        uint8_t buf[3] = {reg, static_cast<uint8_t>(val >> 8), static_cast<uint8_t>(val & 0xFF)};
        return i2c_master_transmit(dev_handle_, buf, 3, pdMS_TO_TICKS(50)) == ESP_OK;
    }

    bool ReadReg8(uint8_t reg, uint8_t* val) {
        return i2c_master_transmit_receive(dev_handle_, &reg, 1, val, 1, pdMS_TO_TICKS(50)) == ESP_OK;
    }

    bool ReadReg8_16(uint8_t reg, uint16_t* val) {
        uint8_t buf[2] = {0};
        if (i2c_master_transmit_receive(dev_handle_, &reg, 1, buf, 2, pdMS_TO_TICKS(50)) != ESP_OK) return false;
        *val = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
        return true;
    }

    // ------------------------------------------------------------------------
    // Low-level I2C helpers for 16-bit register addresses (VL53L1X)
    // ------------------------------------------------------------------------
    bool WriteReg16BitAddr8(uint16_t reg, uint8_t val) {
        uint8_t buf[3] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF), val};
        return i2c_master_transmit(dev_handle_, buf, 3, pdMS_TO_TICKS(50)) == ESP_OK;
    }

    bool WriteReg16BitAddr16(uint16_t reg, uint16_t val) {
        uint8_t buf[4] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF),
                          static_cast<uint8_t>(val >> 8), static_cast<uint8_t>(val & 0xFF)};
        return i2c_master_transmit(dev_handle_, buf, 4, pdMS_TO_TICKS(50)) == ESP_OK;
    }

    bool ReadReg16BitAddr8(uint16_t reg, uint8_t* val) {
        uint8_t reg_buf[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
        return i2c_master_transmit_receive(dev_handle_, reg_buf, 2, val, 1, pdMS_TO_TICKS(50)) == ESP_OK;
    }

    bool ReadReg16BitAddr16(uint16_t reg, uint16_t* val) {
        uint8_t reg_buf[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
        uint8_t data_buf[2] = {0};
        if (i2c_master_transmit_receive(dev_handle_, reg_buf, 2, data_buf, 2, pdMS_TO_TICKS(50)) != ESP_OK) {
            return false;
        }
        *val = (static_cast<uint16_t>(data_buf[0]) << 8) | data_buf[1];
        return true;
    }

    // ------------------------------------------------------------------------
    // VL53L1X Init & Read
    // ------------------------------------------------------------------------
    bool InitVL53L1X() {
        // Soft reset
        WriteReg16BitAddr8(0x0000, 0x00);
        vTaskDelay(pdMS_TO_TICKS(2));
        WriteReg16BitAddr8(0x0000, 0x01);
        vTaskDelay(pdMS_TO_TICKS(2));

        // Poll for boot completion (reg 0x00E5 & 0x01 != 0)
        int64_t start = esp_timer_get_time();
        while (true) {
            uint8_t boot_status = 0;
            if (ReadReg16BitAddr8(0x00E5, &boot_status) && (boot_status & 0x01) != 0) {
                break;
            }
            if ((esp_timer_get_time() - start) > 100000) {  // 100ms
                ESP_LOGW(kTag, "VL53L1X boot completion timed out");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }

        // Set 2V8 mode (reg 0x002F |= 0x01)
        uint8_t pad_cfg = 0;
        ReadReg16BitAddr8(0x002F, &pad_cfg);
        WriteReg16BitAddr8(0x002F, pad_cfg | 0x01);

        // Standard ranging config
        WriteReg16BitAddr8(0x0071, 0x01);  // clear interrupt
        WriteReg16BitAddr8(0x0086, 0x01);  // interrupt clear

        // Start measurement (SYSTEM__MODE_START = 0x40 for timed ranging)
        WriteReg16BitAddr8(0x0087, 0x40);

        initialized_ = true;
        ESP_LOGI(kTag, "VL53L1X initialized successfully!");
        return true;
    }

    uint16_t ReadDistanceL1X() {
        // Wait for sample ready: reg 0x0031 != 0
        uint8_t gpio_status = 0;
        int64_t start = esp_timer_get_time();
        while (true) {
            if (ReadReg16BitAddr8(0x0031, &gpio_status) && (gpio_status & 0x01) == 0) {
                break;  // active low data ready
            }
            if ((esp_timer_get_time() - start) > 60000) {  // 60ms timeout
                return last_distance_mm_;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        // Read distance from RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 (0x0096)
        uint16_t range = 0;
        if (ReadReg16BitAddr16(0x0096, &range)) {
            // Clear interrupt (0x0086 = 0x01)
            WriteReg16BitAddr8(0x0086, 0x01);
            if (range > 4000) range = 4000;
            last_distance_mm_ = range;
            return range;
        }
        return last_distance_mm_;
    }

    // ------------------------------------------------------------------------
    // VL53L0X Init & Read
    // ------------------------------------------------------------------------
    bool InitVL53L0X() {
        WriteReg8(0x88, 0x00);
        WriteReg8(0x80, 0x01);
        WriteReg8(0xFF, 0x01);
        WriteReg8(0x00, 0x00);
        ReadReg8(0x91, &stop_variable_);
        WriteReg8(0x00, 0x01);
        WriteReg8(0xFF, 0x00);
        WriteReg8(0x80, 0x00);

        WriteReg8(0x01, 0xFF);  // sequence config
        WriteReg8(0x0A, 0x04);  // interrupt config
        WriteReg8(0x0B, 0x01);  // clear interrupt

        // Start continuous
        WriteReg8(0x80, 0x01);
        WriteReg8(0xFF, 0x01);
        WriteReg8(0x00, 0x00);
        WriteReg8(0x91, stop_variable_);
        WriteReg8(0x00, 0x01);
        WriteReg8(0xFF, 0x00);
        WriteReg8(0x80, 0x00);
        WriteReg8(0x00, 0x02);

        initialized_ = true;
        ESP_LOGI(kTag, "VL53L0X initialized successfully!");
        return true;
    }

    uint16_t ReadDistanceL0X() {
        uint8_t int_status = 0;
        int64_t start = esp_timer_get_time();
        while (true) {
            if (ReadReg8(0x13, &int_status) && (int_status & 0x07) != 0) break;
            if ((esp_timer_get_time() - start) > 50000) return last_distance_mm_;
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        uint16_t range = 0;
        if (ReadReg8_16(0x1E, &range)) {
            WriteReg8(0x0B, 0x01);
            if (range > 8000) range = 8190;
            last_distance_mm_ = range;
            return range;
        }
        return last_distance_mm_;
    }

    i2c_master_bus_handle_t bus_handle_ = nullptr;
    i2c_master_dev_handle_t dev_handle_ = nullptr;
    uint8_t address_;
    TofModel model_ = TofModel::kUnknown;
    uint8_t stop_variable_ = 0;
    bool initialized_ = false;
    uint16_t last_distance_mm_ = 0;
};
