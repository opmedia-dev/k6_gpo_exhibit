#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================================
// Phone line interface — hook detection and raw current sensing
//
// A small sense resistor sits in series with the DC supply feeding the phone
// line pair.  The ESP32 ADC reads the voltage across it.  When the handset is
// on-hook the circuit is open and the reading is low.  Off-hook draws ~20-30 mA
// producing a clear high reading.  Rotary dial pulses appear as brief drops
// in the reading while the phone is off-hook.
// ============================================================================

enum class HookState : uint8_t {
    ON_HOOK,
    OFF_HOOK
};

class PhoneLine {
public:
    void begin();

    // Call from loop().  Returns the debounced hook state and updates the
    // raw ADC reading available via lastRawReading().
    HookState update();

    HookState hookState() const { return hook_state_; }
    bool      hookChanged() const { return hook_changed_; }
    int       lastRawReading() const { return last_raw_; }

    // True when the line current is interrupted (dial pulse break period).
    bool isLineBreak() const;

    // Averaged raw ADC read (blocking) — used by the calibration routine.
    int  readAveraged(uint16_t samples = 64) const;

    // Detection thresholds. Defaults come from config.h but can be tuned at
    // runtime by the calibration wizard and persisted to settings.json.
    void setThresholds(int on, int off);
    int  thresholdOn()  const { return threshold_on_; }
    int  thresholdOff() const { return threshold_off_; }

    // Threshold used to detect rotary dial-pulse breaks.  A break (line open)
    // must be caught for enough of its ~66 ms duration to be timed reliably.
    // Off-hook the loop reads near full-scale, so we detect a break as a large
    // drop below this level — set higher than the hook OFF threshold so each
    // pulse registers for most of its duration rather than only the bottom
    // sliver as the reading decays.
    int  pulseThreshold() const { return pulse_threshold_; }

    // Compute thresholds from captured on-hook / off-hook raw levels.
    // Returns false if the two levels are too close to separate reliably.
    bool applyCalibration(int onhookRaw, int offhookRaw);

    // True once the line has been calibrated (or a saved calibration loaded),
    // rather than running on the compile-time defaults.
    bool calibrated() const { return calibrated_; }
    void setCalibrated(bool on) { calibrated_ = on; }

private:
    HookState     hook_state_   = HookState::ON_HOOK;
    bool          hook_changed_ = false;
    int           last_raw_     = 0;
    unsigned long last_change_  = 0;
    HookState     pending_      = HookState::ON_HOOK;

    // Runtime-tunable detection thresholds (initialised from config.h).
    int           threshold_on_  = LINE_THRESHOLD_ON;
    int           threshold_off_ = LINE_THRESHOLD_OFF;
    int           pulse_threshold_ = LINE_THRESHOLD_ON;
    bool          calibrated_    = false;

    // Line-sense debug state (used only when g_line_debug is true).
    bool          dbg_break_    = false;
    unsigned long dbg_edge_ms_  = 0;
    unsigned long dbg_last_ms_  = 0;
};
