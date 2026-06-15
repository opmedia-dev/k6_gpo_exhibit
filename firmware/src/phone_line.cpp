#include "phone_line.h"
#include "config.h"

void PhoneLine::begin() {
    analogSetAttenuation(ADC_11db);  // full 0-3.3 V range
    pinMode(PIN_LINE_SENSE, INPUT);
    hook_state_  = HookState::ON_HOOK;
    pending_     = HookState::ON_HOOK;
    last_change_ = millis();
}

HookState PhoneLine::update() {
    last_raw_ = analogRead(PIN_LINE_SENSE);

    HookState sample = (last_raw_ >= LINE_THRESHOLD_ON)
                        ? HookState::OFF_HOOK
                        : HookState::ON_HOOK;

    // Hysteresis: once off-hook, require a lower threshold to go on-hook
    if (hook_state_ == HookState::OFF_HOOK && last_raw_ > LINE_THRESHOLD_OFF) {
        sample = HookState::OFF_HOOK;
    }

    hook_changed_ = false;

    if (sample != pending_) {
        pending_     = sample;
        last_change_ = millis();
    } else if (pending_ != hook_state_ &&
               (millis() - last_change_ >= HOOK_DEBOUNCE_MS)) {
        hook_state_  = pending_;
        hook_changed_ = true;
    }

    return hook_state_;
}

bool PhoneLine::isLineBreak() const {
    return (last_raw_ < LINE_THRESHOLD_OFF);
}
