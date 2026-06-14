#pragma once

#include <Arduino.h>
#include "config.h"
#include "phone_line.h"
#include "rotary_decoder.h"
#include "bell_driver.h"
#include "audio_player.h"
#include "control_panel.h"
#include "coin_box.h"

// ============================================================================
// Phone controller — exhibit state machine
//
// States:
//   IDLE             On-hook.  Auto-ring timer ticking.
//   RINGING          Bell ringing (auto or manual).  Awaiting pickup.
//   PLAYING_HISTORY  Handset answered a ring → playing a history track.
//   DIAL_TONE        Handset lifted without ring → dial tone plays.
//   DIALING          Digits accumulating from rotary dial.
//   AWAIT_COINS      (A+B only) Number recognised, waiting for coins.
//   AWAIT_BTN_A      (A+B only) Call connected, waiting for Button A.
//   AWAIT_BTN_B      (A+B only) Number not recognised, waiting for Button B.
//   RINGING_TONE    Outgoing call → playing UK ringing tone before connecting.
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
    AWAIT_COINS,
    AWAIT_BTN_A,
    AWAIT_BTN_B,
    RINGING_TONE,
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
    void setAutoRing(bool enabled);
    bool autoRingEnabled() const   { return auto_ring_enabled_; }
    void toggleAutoRing()          { setAutoRing(!auto_ring_enabled_); }

    // Configurable auto-ring interval (milliseconds).
    void setAutoRingInterval(unsigned long minMs, unsigned long maxMs);
    unsigned long autoRingMinMs() const { return auto_ring_min_ms_; }
    unsigned long autoRingMaxMs() const { return auto_ring_max_ms_; }

    // Configurable max ring cadences (0 = unlimited).
    void setMaxRingCadences(int n) { max_ring_cadences_ = n; }
    int  maxRingCadences() const { return max_ring_cadences_; }

    // Configurable ringing tone duration range (seconds) for outgoing calls.
    void setRingToneRange(int minSecs, int maxSecs);
    int  ringToneMinSecs() const { return ring_tone_min_ms_ / 1000; }
    int  ringToneMaxSecs() const { return ring_tone_max_ms_ / 1000; }

    PhoneLine&      line()    { return line_; }
    BellDriver&     bell()    { return bell_; }
    AudioPlayer&    player()  { return player_; }
    RotaryDecoder&  dial()    { return dial_; }
    ControlPanel&   panel()   { return panel_; }
    CoinBox&        coinBox() { return coin_box_; }

    const char* dialledNumber() const { return dialled_; }
    unsigned long stateEnterTime() const { return state_enter_time_; }

private:
    void enterState(PhoneState s);
    void resetAutoRingTimer();
    void updateLamp();

    PhoneState     state_ = PhoneState::IDLE;
    PhoneLine      line_;
    RotaryDecoder  dial_;
    BellDriver     bell_;
    AudioPlayer    player_;
    ControlPanel   panel_;
    CoinBox        coin_box_;

    char           dialled_[MAX_DIALLED_DIGITS + 1] = {};
    char           pending_path_[64] = {};  // path to play after coins inserted
    uint8_t        dial_pos_ = 0;

    unsigned long  state_enter_time_ = 0;
    unsigned long  last_digit_time_  = 0;
    bool           replace_prompted_ = false;  // "replace handset" already played

    // Auto-ring
    bool           auto_ring_enabled_ = true;
    unsigned long  next_ring_time_    = 0;
    unsigned long  auto_ring_min_ms_  = AUTO_RING_MIN_MS;
    unsigned long  auto_ring_max_ms_  = AUTO_RING_MAX_MS;
    int            max_ring_cadences_ = DEFAULT_MAX_RING_CADENCES;
    int            ring_cadence_count_ = 0;
    unsigned long  ring_tone_min_ms_ = DEFAULT_RING_TONE_MS;
    unsigned long  ring_tone_max_ms_ = DEFAULT_RING_TONE_MS;
    unsigned long  ring_tone_target_ = DEFAULT_RING_TONE_MS;  // randomised per call

    DigitCallback  digit_cb_  = nullptr;
    NumberCallback number_cb_ = nullptr;
    HookCallback   hook_cb_   = nullptr;
    StateCallback  state_cb_  = nullptr;
};
