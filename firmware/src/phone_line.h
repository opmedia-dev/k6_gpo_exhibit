#pragma once

#include <Arduino.h>

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

private:
    HookState     hook_state_   = HookState::ON_HOOK;
    bool          hook_changed_ = false;
    int           last_raw_     = 0;
    unsigned long last_change_  = 0;
    HookState     pending_      = HookState::ON_HOOK;

    // Line-sense debug state (used only when g_line_debug is true).
    bool          dbg_break_    = false;
    unsigned long dbg_edge_ms_  = 0;
    unsigned long dbg_last_ms_  = 0;
};
