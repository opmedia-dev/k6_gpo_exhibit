#include "bell_driver.h"
#include "config.h"

// Cadence table: duration (ms) and whether the bell is active during that step.
static const struct { unsigned long duration; bool active; } CADENCE[] = {
    { RING_ON_1_MS,  true  },
    { RING_OFF_1_MS, false },
    { RING_ON_2_MS,  true  },
    { RING_OFF_2_MS, false },
};
static constexpr int CADENCE_STEPS = sizeof(CADENCE) / sizeof(CADENCE[0]);

void BellDriver::begin() {
    pinMode(PIN_RING_EN, OUTPUT);
    pinMode(PIN_RING_A,  OUTPUT);
    pinMode(PIN_RING_B,  OUTPUT);
    setBridgeOff();
}

void BellDriver::startRinging() {
    if (ringing_) return;
    ringing_       = true;
    cadence_step_  = 0;
    cadence_start_ = millis();
    toggle_time_   = millis();
    phase_         = false;
}

void BellDriver::stopRinging() {
    ringing_ = false;
    setBridgeOff();
}

void BellDriver::update() {
    if (!ringing_) return;

    unsigned long now = millis();

    // Advance cadence step when the current step duration has elapsed.
    if (now - cadence_start_ >= CADENCE[cadence_step_].duration) {
        cadence_start_ = now;
        cadence_step_  = (cadence_step_ + 1) % CADENCE_STEPS;
    }

    if (CADENCE[cadence_step_].active) {
        // Toggle H-bridge at RING_FREQ_HZ (25 Hz → 20 ms half-period).
        unsigned long halfPeriod = 500 / RING_FREQ_HZ;  // 500 ms / 25 = 20 ms
        if (now - toggle_time_ >= halfPeriod) {
            toggle_time_ = now;
            phase_ = !phase_;
            setBridgeOutput(phase_);
        }
    } else {
        setBridgeOff();
    }
}

void BellDriver::setBridgeOutput(bool phaseA) {
    digitalWrite(PIN_RING_EN, HIGH);
    if (phaseA) {
        digitalWrite(PIN_RING_A, HIGH);
        digitalWrite(PIN_RING_B, LOW);
    } else {
        digitalWrite(PIN_RING_A, LOW);
        digitalWrite(PIN_RING_B, HIGH);
    }
}

void BellDriver::setBridgeOff() {
    digitalWrite(PIN_RING_EN, LOW);
    digitalWrite(PIN_RING_A,  LOW);
    digitalWrite(PIN_RING_B,  LOW);
}
