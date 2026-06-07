#pragma once

#include <Arduino.h>
#include "phone_line.h"
#include "rotary_decoder.h"
#include "bell_driver.h"
#include "audio_interface.h"

// ============================================================================
// Phone controller — top-level state machine
//
// States:
//   IDLE        Phone on-hook; waiting for incoming ring or handset lift.
//   RINGING     Bell ringing (triggered externally). Stops on off-hook.
//   DIAL_TONE   Handset lifted; continuous dial tone plays.
//   DIALING     Rotary dial pulses detected; tone stops, digits accumulate.
//   CONNECTED   Dialling complete; call active (audio pass-through).
//   BUSY        Error / no route — busy tone plays until on-hook.
//
// The controller owns the other subsystem objects and drives them each loop().
// Application code can trigger an incoming ring or inject audio by calling
// the public API.
// ============================================================================

// Maximum number of dialled digits stored
constexpr int MAX_DIALLED_DIGITS = 20;

enum class PhoneState : uint8_t {
    IDLE,
    RINGING,
    DIAL_TONE,
    DIALING,
    CONNECTED,
    BUSY
};

// Callback typedefs for application integration
using DigitCallback   = void (*)(uint8_t digit);
using NumberCallback  = void (*)(const char* number);
using HookCallback    = void (*)(HookState state);

class PhoneController {
public:
    void begin();
    void update();  // call every loop()

    PhoneState state() const { return state_; }

    // Trigger an incoming ring (ignored if phone is off-hook).
    void ring();

    // Hang up (end call / stop ringing) from the application side.
    void hangUp();

    // Register callbacks
    void onDigit(DigitCallback cb)   { digit_cb_  = cb; }
    void onNumber(NumberCallback cb) { number_cb_ = cb; }
    void onHook(HookCallback cb)     { hook_cb_   = cb; }

    // Access subsystems
    PhoneLine&      line()  { return line_; }
    BellDriver&     bell()  { return bell_; }
    AudioInterface& audio() { return audio_; }
    RotaryDecoder&  dial()  { return dial_; }

    const char* dialledNumber() const { return dialled_; }

private:
    void enterState(PhoneState s);

    PhoneState     state_ = PhoneState::IDLE;
    PhoneLine      line_;
    RotaryDecoder  dial_;
    BellDriver     bell_;
    AudioInterface audio_;

    char           dialled_[MAX_DIALLED_DIGITS + 1] = {};
    uint8_t        dial_pos_ = 0;

    unsigned long  state_enter_time_ = 0;
    unsigned long  last_digit_time_  = 0;

    DigitCallback  digit_cb_  = nullptr;
    NumberCallback number_cb_ = nullptr;
    HookCallback   hook_cb_   = nullptr;
};
