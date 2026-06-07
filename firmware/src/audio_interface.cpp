#include "audio_interface.h"
#include "config.h"
#include <math.h>

void AudioInterface::begin() {
    // DAC pin is configured automatically by dacWrite().
    pinMode(PIN_AUDIO_IN, INPUT);
    analogSetAttenuation(ADC_11db);
}

void AudioInterface::writeSample(uint8_t sample) {
    dacWrite(PIN_AUDIO_OUT, sample);
}

int AudioInterface::readSample() {
    return analogRead(PIN_AUDIO_IN);
}

// Generate a dual-tone signal (like UK dial tone: 350 + 440 Hz combined).
void AudioInterface::playDialTone(unsigned long durationMs) {
    playing_    = true;
    play_start_ = millis();
    play_dur_   = durationMs;

    unsigned long start = millis();
    while (millis() - start < durationMs && playing_) {
        float t = (millis() - start) / 1000.0f;
        // Two-tone mix, centred at 128 (mid-range of 8-bit DAC)
        float s = 64.0f * sinf(2.0f * M_PI * 350.0f * t)
                + 64.0f * sinf(2.0f * M_PI * 440.0f * t);
        uint8_t out = constrain((int)(128.0f + s), 0, 255);
        dacWrite(PIN_AUDIO_OUT, out);
        delayMicroseconds(125);  // ~8 kHz sample rate
    }
    dacWrite(PIN_AUDIO_OUT, 128);  // silence = mid-rail
    playing_ = false;
}

void AudioInterface::playBusyTone(int count) {
    playing_ = true;
    for (int i = 0; i < count && playing_; i++) {
        unsigned long start = millis();
        while (millis() - start < 375 && playing_) {
            float t = (millis() - start) / 1000.0f;
            float s = 80.0f * sinf(2.0f * M_PI * 400.0f * t);
            uint8_t out = constrain((int)(128.0f + s), 0, 255);
            dacWrite(PIN_AUDIO_OUT, out);
            delayMicroseconds(125);
        }
        dacWrite(PIN_AUDIO_OUT, 128);
        if (playing_) delay(375);
    }
    playing_ = false;
}

void AudioInterface::playDigitTone(uint8_t digit, unsigned long durationMs) {
    // Simple single-tone beep whose frequency depends on the digit.
    // Purely for audible feedback — not DTMF signalling.
    float freq = 400.0f + digit * 40.0f;
    playing_ = true;

    unsigned long start = millis();
    while (millis() - start < durationMs && playing_) {
        float t = (millis() - start) / 1000.0f;
        float s = 80.0f * sinf(2.0f * M_PI * freq * t);
        uint8_t out = constrain((int)(128.0f + s), 0, 255);
        dacWrite(PIN_AUDIO_OUT, out);
        delayMicroseconds(125);
    }
    dacWrite(PIN_AUDIO_OUT, 128);
    playing_ = false;
}

void AudioInterface::stopTone() {
    playing_ = false;
    dacWrite(PIN_AUDIO_OUT, 128);
}
