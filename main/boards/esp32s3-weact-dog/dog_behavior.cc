#include "dog_behavior.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cstring>

#include "application.h"

#define TAG "DogBehavior"

// ============================================================================
// THE mapping table: server emotion -> eyes + legs.
// Unknown strings fall back to kNeutral / no motion (never crash).
// ============================================================================
namespace {
struct EmotionRule {
    const char* emotion;
    EyePreset eyes;
    LegMotion legs;
};

const EmotionRule kEmotionTable[] = {
    // server emotions (xiaozhi LLM emoji set)
    {"happy", EyePreset::kHappy, LegMotion::kWiggle},
    {"laughing", EyePreset::kLaugh, LegMotion::kWave},
    {"funny", EyePreset::kHappy, LegMotion::kWiggle},
    {"loving", EyePreset::kHappy, LegMotion::kWiggle},
    {"delicious", EyePreset::kHappy, LegMotion::kWiggle},
    {"sad", EyePreset::kSad, LegMotion::kCrouch},
    {"crying", EyePreset::kSad, LegMotion::kCrouch},
    {"embarrassed", EyePreset::kSad, LegMotion::kNone},
    {"angry", EyePreset::kAngry, LegMotion::kNone},
    {"surprised", EyePreset::kSurprised, LegMotion::kQuickStand},
    {"shocked", EyePreset::kSurprised, LegMotion::kQuickStand},
    {"confused", EyePreset::kConfused, LegMotion::kNone},
    {"thinking", EyePreset::kConfused, LegMotion::kNone},
    {"sleepy", EyePreset::kSleepy, LegMotion::kNone},
    {"relaxed", EyePreset::kSleepy, LegMotion::kNone},
    // Not in the brief, mild positive faces: happy eyes, no legs (deviation, see PLAN.md)
    {"winking", EyePreset::kHappy, LegMotion::kNone},
    {"cool", EyePreset::kHappy, LegMotion::kNone},
    {"confident", EyePreset::kHappy, LegMotion::kNone},
    {"kissy", EyePreset::kHappy, LegMotion::kNone},
    {"silly", EyePreset::kHappy, LegMotion::kNone},
    // device-side emotions sent by the firmware core
    {"robot_2", EyePreset::kBoot, LegMotion::kNone},
    {"warning", EyePreset::kError, LegMotion::kNone},
    {"cloud_off", EyePreset::kError, LegMotion::kNone},
    {"cancel", EyePreset::kError, LegMotion::kNone},
    {"gear", EyePreset::kError, LegMotion::kNone},  // Wi-Fi provisioning
    {"link", EyePreset::kNeutral, LegMotion::kNone},  // activation code shown
};

// ============================================================================
// Device state -> eyes + legs
// ============================================================================
struct StateRule {
    DeviceState state;
    EyePreset eyes;
    LegMotion legs;
};

const StateRule kStateTable[] = {
    {kDeviceStateStarting, EyePreset::kBoot, LegMotion::kNone},
    {kDeviceStateIdle, EyePreset::kIdle, LegMotion::kStand},
    {kDeviceStateConnecting, EyePreset::kNeutral, LegMotion::kNone},
    {kDeviceStateListening, EyePreset::kListening, LegMotion::kStand},
    {kDeviceStateSpeaking, EyePreset::kSpeaking, LegMotion::kSoftWiggle},
    {kDeviceStateWifiConfiguring, EyePreset::kError, LegMotion::kNone},
    {kDeviceStateFatalError, EyePreset::kError, LegMotion::kRelax},
};
// "Thinking" is not an upstream state: Listening + user text received.
constexpr EyePreset kThinkingEyes = EyePreset::kThinking;

int64_t now_ms() { return esp_timer_get_time() / 1000; }
}  // namespace

DogBehavior::DogBehavior(EyesDisplay* eyes, LegServos* legs) : eyes_(eyes), legs_(legs) {
    events_ = xQueueCreate(8, sizeof(Event));
}

void DogBehavior::Start() {
    eyes_->SetEmotionHook([this](const char* e) { OnEmotion(e); });
    eyes_->SetChatHook([this](const char* role, const char* content) { OnChat(role, content); });
    eyes_->ApplyPreset(EyePreset::kBoot);
    xTaskCreate(&DogBehavior::TaskEntry, "dog", 4096, this, 3, &task_);
}

void DogBehavior::OnEmotion(const char* emotion) {
    Event ev = {};
    ev.type = EventType::kEmotion;
    strncpy(ev.text, emotion ? emotion : "", sizeof(ev.text) - 1);
    xQueueSend(events_, &ev, 0);
}

void DogBehavior::OnChat(const char* role, const char* content) {
    if (role == nullptr || strcmp(role, "user") != 0 || content == nullptr || content[0] == 0) return;
    Event ev = {};
    ev.type = EventType::kUserText;
    xQueueSend(events_, &ev, 0);
}

void DogBehavior::TaskEntry(void* arg) { static_cast<DogBehavior*>(arg)->Loop(); }

void DogBehavior::HandleState(DeviceState state, int64_t now) {
    ESP_LOGI(TAG, "state %d -> %d", (int)state_, (int)state);
    state_ = state;
    state_since_ms_ = now;
    thinking_ = false;
    surprise_until_ms_ = 0;

    // Servos are powered only once Wi-Fi and audio are up (first Idle).
    if (state == kDeviceStateIdle && !servos_started_) {
        servos_started_ = true;
        legs_->AttachSequential();
    }

    for (const auto& r : kStateTable) {
        if (r.state != state) continue;
        eyes_->ApplyPreset(r.eyes);
        if (r.legs == LegMotion::kStand && sat_down_) sat_down_ = false;
        legs_->Play(r.legs);
        if (r.legs == LegMotion::kSoftWiggle) last_wiggle_ms_ = now;
        return;
    }
}

void DogBehavior::HandleEmotion(const char* emotion, int64_t now) {
    if (strcmp(emotion, "neutral") == 0 || emotion[0] == 0) {
        // The core sends "neutral" on every idle/listening/connecting change;
        // the state table owns those looks. Only reset the mood while speaking.
        if (state_ == kDeviceStateSpeaking) eyes_->ApplyPreset(EyePreset::kNeutral);
        return;
    }
    for (const auto& r : kEmotionTable) {
        if (strcmp(r.emotion, emotion) != 0) continue;
        eyes_->ApplyPreset(r.eyes);
        if (r.eyes == EyePreset::kSurprised) surprise_until_ms_ = now + DOG_SURPRISE_MS;
        legs_->Play(r.legs);
        if (r.legs != LegMotion::kNone) last_wiggle_ms_ = now;  // don't overlap a soft wiggle
        return;
    }
    ESP_LOGW(TAG, "unknown emotion '%s' -> neutral", emotion);
    eyes_->ApplyPreset(EyePreset::kNeutral);
}

void DogBehavior::Loop() {
    // Let Board/Application construction finish before touching the singleton.
    vTaskDelay(pdMS_TO_TICKS(500));
    auto& app = Application::GetInstance();
    TickType_t last = xTaskGetTickCount();
    while (true) {
        int64_t now = now_ms();
        DeviceState s = app.GetDeviceState();
        if (s != state_) HandleState(s, now);

        Event ev;
        while (xQueueReceive(events_, &ev, 0) == pdTRUE) {
            if (ev.type == EventType::kEmotion) {
                HandleEmotion(ev.text, now);
            } else if (ev.type == EventType::kUserText && state_ == kDeviceStateListening &&
                       !thinking_) {
                thinking_ = true;
                eyes_->ApplyPreset(kThinkingEyes);
            }
        }

        if (surprise_until_ms_ != 0 && now > surprise_until_ms_) {
            surprise_until_ms_ = 0;
            eyes_->SetCuriosity(false);
        }
        if (state_ == kDeviceStateSpeaking && !legs_->busy() &&
            now - last_wiggle_ms_ > DOG_SPEAK_WIGGLE_EVERY_MS) {
            last_wiggle_ms_ = now;
            legs_->Play(LegMotion::kSoftWiggle);
        }
        if (state_ == kDeviceStateIdle && !sat_down_ && legs_->attached() &&
            now - state_since_ms_ > DOG_IDLE_SIT_AFTER_MS) {
            sat_down_ = true;
            legs_->Play(LegMotion::kSit);  // PWM is cut automatically after the hold
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(DOG_POLL_MS));
    }
}
