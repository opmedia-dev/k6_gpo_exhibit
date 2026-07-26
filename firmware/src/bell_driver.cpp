#include "bell_driver.h"
#include "config.h"

static const int LEDC_CHANNEL  = 0;
static const int LEDC_FREQ     = 1000;  // 1 kHz PWM carrier (inaudible)
static const int LEDC_RES_BITS = 8;     // 0-255 duty range

// Cadence table: duration (ms) and whether the bell is active during that step.
static const struct { unsigned long duration; bool active; } CADENCE[] = {
    { RING_ON_1_MS,  true  },
    { RING_OFF_1_MS, false },
    { RING_ON_2_MS,  true  },
    { RING_OFF_2_MS, false },
};
static constexpr int CADENCE_STEPS = sizeof(CADENCE) / sizeof(CADENCE[0]);

void BellDriver::begin() {
    ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RES_BITS);
    ledcAttachPin(PIN_RING_EN, LEDC_CHANNEL);
    ledcWrite(LEDC_CHANNEL, 0);
    pinMode(PIN_RING_A,  OUTPUT);
    pinMode(PIN_RING_B,  OUTPUT);
    setBridgeOff();
}

void BellDriver::setBellVolume(uint8_t vol) {
    bell_volume_ = vol;
}

void BellDriver::setRingFreq(int hz) {
    if (hz < 10) hz = 10;
    if (hz > 50) hz = 50;
    ring_freq_hz_ = hz;
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
        // Toggle H-bridge at ring_freq_hz_ (e.g. 25 Hz → 20 ms half-period).
        unsigned long halfPeriod = 500 / ring_freq_hz_;
        if (now - toggle_time_ >= halfPeriod) {
            toggle_time_ = now;
            phase_ = !phase_;
            setBridgeOutput(phase_);
        }
    } else {
        setBridgeOff();
    }
}

bool BellDriver::inSilentGap(unsigned long guardMs) const {
    if (!ringing_) return true;                       // not ringing — line clean
    if (CADENCE[cadence_step_].active) return false;  // bell striking — coupling
    return (millis() - cadence_start_) >= guardMs;     // settled silent gap
}

void BellDriver::strike(unsigned long durationMs) {
    unsigned long start = millis();
    bool ph = false;
    unsigned long halfPeriod = 500 / ring_freq_hz_;
    unsigned long lastToggle = start;
    while (millis() - start < durationMs) {
        if (millis() - lastToggle >= halfPeriod) {
            lastToggle = millis();
            ph = !ph;
            setBridgeOutput(ph);
        }
    }
    setBridgeOff();
}

void BellDriver::setBridgeOutput(bool phaseA) {
    ledcWrite(LEDC_CHANNEL, bell_volume_);
    if (phaseA) {
        digitalWrite(PIN_RING_A, HIGH);
        digitalWrite(PIN_RING_B, LOW);
    } else {
        digitalWrite(PIN_RING_A, LOW);
        digitalWrite(PIN_RING_B, HIGH);
    }
}

void BellDriver::setBridgeOff() {
    ledcWrite(LEDC_CHANNEL, 0);
    digitalWrite(PIN_RING_A,  LOW);
    digitalWrite(PIN_RING_B,  LOW);
}
