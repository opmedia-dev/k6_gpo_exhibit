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
        bool brk = (last_raw_ < LINE_THRESHOLD_OFF);
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
