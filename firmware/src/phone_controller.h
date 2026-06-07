#pragma once

#include <Arduino.h>
#include "config.h"
#include "phone_line.h"
#include "rotary_decoder.h"
#include "bell_driver.h"
#include "audio_player.h"
#include "control_panel.h"

// ============================================================================
// Phone controller — exhibit state machine
//
// States:
//   IDLE             On-hook.  Auto-ring timer ticking.
//   RINGING          Bell ringing (auto or manual).  Awaiting pickup.
//   PLAYING_HISTORY  Handset answered a ring → playing a history track.
//   DIAL_TONE        Handset lifted without ring → dial tone plays.
//   DIALING          Digits accumulating from rotary dial.
//   PLAYING_NUMBER   Dialling complete → matched MP3 playing.
//   PLAYING_NOT_REC  Dialling complete → "number not recognised" playing.
//   BUSY             Error / timeout → busy tone.
// ============================================================================

enum class PhoneState : uint8_t {
    IDLE,
    RINGING,
    PLAYING_HISTORY,
    DIAL_TONE,
    DIALING,
    PLAYING_NUMBER,
    PLAYING_NOT_REC,
    BUSY
};

using DigitCallback   = void (*)(uint8_t digit);
using NumberCallback  = void (*)(const char* number);
using HookCallback    = void (*)(HookState state);
using StateCallback   = void (*)(PhoneState state);

class PhoneController {
public:
    void begin();
    void update();

    PhoneState state() const { return state_; }
    const char* stateName() const;

    void ring();
    void cancelRing();
    void hangUp();

    void onDigit(DigitCallback cb)   { digit_cb_  = cb; }
    void onNumber(NumberCallback cb) { number_cb_ = cb; }
    void onHook(HookCallback cb)     { hook_cb_   = cb; }
    void onState(StateCallback cb)   { state_cb_  = cb; }

    // Enable / disable the random auto-ring feature.
    void setAutoRing(bool enabled) { auto_ring_enabled_ = enabled; }
    bool autoRingEnabled() const   { return auto_ring_enabled_; }

    PhoneLine&      line()    { return line_; }
    BellDriver&     bell()    { return bell_; }
    AudioPlayer&    player()  { return player_; }
    RotaryDecoder&  dial()    { return dial_; }
    ControlPanel&   panel()   { return panel_; }

    const char* dialledNumber() const { return dialled_; }

private:
    void enterState(PhoneState s);
    void resetAutoRingTimer();

    PhoneState     state_ = PhoneState::IDLE;
    PhoneLine      line_;
    RotaryDecoder  dial_;
    BellDriver     bell_;
    AudioPlayer    player_;
    ControlPanel   panel_;

    char           dialled_[MAX_DIALLED_DIGITS + 1] = {};
    uint8_t        dial_pos_ = 0;

    unsigned long  state_enter_time_ = 0;
    unsigned long  last_digit_time_  = 0;

    // Auto-ring
    bool           auto_ring_enabled_ = true;
    unsigned long  next_ring_time_    = 0;

    DigitCallback  digit_cb_  = nullptr;
    NumberCallback number_cb_ = nullptr;
    HookCallback   hook_cb_   = nullptr;
    StateCallback  state_cb_  = nullptr;
};
