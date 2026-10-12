// DogBehavior: maps device state + server emotions to eyes and legs.
// All mapping tables live in dog_behavior.cc.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "device_state.h"
#include "eyes_display.h"
#include "leg_servos.h"

class DogBehavior {
public:
    DogBehavior(EyesDisplay* eyes, LegServos* legs);
    void Start();
    void SetDistanceSensor(class VL53L0X* sensor) { sensor_ = sensor; }

    // Called from EyesDisplay hooks (any task). Non-blocking.
    void OnEmotion(const char* emotion);
    void OnChat(const char* role, const char* content);

private:
    enum class EventType : uint8_t { kEmotion, kUserText };
    struct Event {
        EventType type;
        char text[24];
    };

    static void TaskEntry(void* arg);
    void Loop();
    void HandleState(DeviceState state, int64_t now);
    void HandleEmotion(const char* emotion, int64_t now);

    EyesDisplay* eyes_;
    LegServos* legs_;
    class VL53L0X* sensor_ = nullptr;
    QueueHandle_t events_ = nullptr;
    TaskHandle_t task_ = nullptr;

    DeviceState state_ = kDeviceStateUnknown;
    bool servos_started_ = false;
    bool thinking_ = false;
    bool sat_down_ = false;
    int64_t state_since_ms_ = 0;
    int64_t last_wiggle_ms_ = 0;
    int64_t surprise_until_ms_ = 0;
    int64_t last_proximity_trigger_ms_ = 0;
};
