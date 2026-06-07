#pragma once

#include <Arduino.h>

// ============================================================================
// Rotary dial pulse decoder
//
// Monitors the line-break signal produced by PhoneLine and counts pulses.
// A GPO rotary dial produces N pulses per digit at ~10 pps with a 2:1
// break/make ratio.  After an inter-digit gap the accumulated count is
// emitted as a decoded digit (1-9 → 1-9, 10 → 0).
// ============================================================================

class RotaryDecoder {
public:
    void begin();

    // Feed the current line-break state every loop iteration.
    // Returns true when a complete digit has been decoded.
    bool update(bool lineBreak);

    // The last decoded digit (0-9).  Valid only after update() returns true.
    uint8_t digit() const { return decoded_digit_; }

    // Number of raw pulses counted so far in the current digit.
    uint8_t pulseCount() const { return pulse_count_; }

    // Reset the decoder (e.g. on hook-on).
    void reset();

private:
    enum class State : uint8_t {
        IDLE,
        IN_BREAK,
        IN_MAKE,
        WAIT_NEXT_PULSE
    };

    State         state_         = State::IDLE;
    uint8_t       pulse_count_   = 0;
    uint8_t       decoded_digit_ = 0;
    unsigned long edge_time_     = 0;
    bool          prev_break_    = false;
};
