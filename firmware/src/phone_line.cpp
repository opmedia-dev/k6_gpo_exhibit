#include "phone_line.h"
#include "config.h"

// When true, log the raw line reading and every make/break edge so rotary
// dial pulses can be observed live over serial (toggled by the 'N' command).
volatile bool g_line_debug = false;

void PhoneLine::begin() {
    analogSetAttenuation(ADC_11db);  // full 0-3.3 V range
    pinMode(PIN_LINE_SENSE, INPUT);
    hook_state_  = HookState::ON_HOOK;
    pending_     = HookState::ON_HOOK;
    last_change_ = millis();
}

HookState PhoneLine::update() {
    last_raw_ = analogRead(PIN_LINE_SENSE);

    if (g_line_debug) {
        bool brk = (last_raw_ < threshold_off_);
        unsigned long now = millis();
        if (brk != dbg_break_) {
            // Edge: report the reading and, on a make, how long the break was.
            if (brk) {
                Serial.printf("[linedbg] BREAK  raw=%d\n", last_raw_);
            } else {
                Serial.printf("[linedbg] make   raw=%d  break_len=%lums\n",
                              last_raw_, now - dbg_edge_ms_);
            }
            dbg_break_   = brk;
            dbg_edge_ms_ = now;
        }
        if (now - dbg_last_ms_ >= 500) {
            Serial.printf("[linedbg] raw=%d  hook=%s\n", last_raw_,
                          hook_state_ == HookState::OFF_HOOK ? "OFF" : "ON");
            dbg_last_ms_ = now;
        }
    }

    HookState sample = (last_raw_ >= threshold_on_)
                        ? HookState::OFF_HOOK
                        : HookState::ON_HOOK;

    // Hysteresis: once off-hook, require a lower threshold to go on-hook
    if (hook_state_ == HookState::OFF_HOOK && last_raw_ > threshold_off_) {
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
    return (last_raw_ < threshold_off_);
}

int PhoneLine::readAveraged(uint16_t samples) const {
    if (samples == 0) samples = 1;
    uint32_t sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        sum += analogRead(PIN_LINE_SENSE);
        delay(1);
    }
    return (int)(sum / samples);
}

void PhoneLine::setThresholds(int on, int off) {
    // Keep on > off so the hysteresis band is well-formed.
    if (off >= on) off = on - 1;
    if (off < 0)   off = 0;
    threshold_on_  = on;
    threshold_off_ = off;
}

bool PhoneLine::applyCalibration(int onhookRaw, int offhookRaw) {
    // Off-hook (loop closed, opto on) must read clearly higher than on-hook.
    int span = offhookRaw - onhookRaw;
    if (span < 300) return false;  // too little separation to trust

    // Place the hysteresis band inside the gap: ON at ~2/3, OFF at ~1/3.
    int on  = onhookRaw + (span * 2) / 3;
    int off = onhookRaw + span / 3;
    setThresholds(on, off);
    return true;
}
