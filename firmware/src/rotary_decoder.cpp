#include "rotary_decoder.h"
#include "config.h"

void RotaryDecoder::begin() {
    reset();
}

void RotaryDecoder::reset() {
    state_         = State::IDLE;
    pulsed_        = false;
    pulse_count_   = 0;
    decoded_digit_ = 0;
    edge_time_     = 0;
    prev_break_    = false;
}

bool RotaryDecoder::update(bool lineBreak) {
    unsigned long now = millis();
    bool digitReady   = false;
    pulsed_           = false;

    switch (state_) {
    case State::IDLE:
        if (lineBreak && !prev_break_) {
            // Falling edge — start of a break pulse
            edge_time_ = now;
            state_     = State::IN_BREAK;
        }
        break;

    case State::IN_BREAK:
        if (!lineBreak) {
            // Rising edge — break ended
            unsigned long breakLen = now - edge_time_;
            if (breakLen >= PULSE_MIN_BREAK_MS && breakLen <= PULSE_MAX_BREAK_MS) {
                pulse_count_++;
                pulsed_ = true;
            }
            edge_time_ = now;
            state_     = State::IN_MAKE;
        } else if (now - edge_time_ > PULSE_MAX_BREAK_MS) {
            // Break too long — not a dial pulse; abort
            if (pulse_count_ > 0) {
                decoded_digit_ = (pulse_count_ >= 10) ? 0 : pulse_count_;
                digitReady     = true;
                pulse_count_   = 0;
            }
            state_ = State::IDLE;
        }
        break;

    case State::IN_MAKE:
        if (lineBreak && !prev_break_) {
            // Next break starts
            edge_time_ = now;
            state_     = State::IN_BREAK;
        } else if (now - edge_time_ > INTER_DIGIT_TIMEOUT_MS) {
            // Inter-digit timeout — digit complete
            if (pulse_count_ > 0) {
                decoded_digit_ = (pulse_count_ >= 10) ? 0 : pulse_count_;
                digitReady     = true;
                pulse_count_   = 0;
            }
            state_ = State::IDLE;
        }
        break;

    default:
        state_ = State::IDLE;
        break;
    }

    prev_break_ = lineBreak;
    return digitReady;
}
