#pragma once

#include <Arduino.h>

// ============================================================================
// Audio interface — telephone-quality audio I/O over the phone line
//
// Transmit path (earpiece):
//   ESP32 DAC → coupling transformer → phone line → induction coil → earpiece
//
// Receive path (microphone):
//   Carbon mic → induction coil → phone line → coupling cap → ESP32 ADC
//
// This first revision uses the ESP32's built-in 8-bit DAC and 12-bit ADC at
// 8 kHz.  For higher fidelity, swap to an I2S codec (e.g. MAX98357A output,
// INMP441 input) — the AudioInterface API stays the same.
// ============================================================================

class AudioInterface {
public:
    void begin();

    // Write a single 8-bit sample to the DAC (earpiece).
    void writeSample(uint8_t sample);

    // Read a 12-bit sample from the ADC (microphone).
    int readSample();

    // Play a generated dial-tone burst (350 + 440 Hz) for the given duration.
    void playDialTone(unsigned long durationMs);

    // Play a busy tone (400 Hz, 375 ms on / 375 ms off) for the given count.
    void playBusyTone(int count);

    // Play a single DTMF-like feedback beep.
    void playDigitTone(uint8_t digit, unsigned long durationMs);

    // Stop any playing tone.
    void stopTone();

    bool isPlaying() const { return playing_; }

private:
    bool          playing_    = false;
    unsigned long play_start_ = 0;
    unsigned long play_dur_   = 0;
};
