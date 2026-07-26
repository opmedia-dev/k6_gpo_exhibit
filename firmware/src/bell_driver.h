#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================================
// Bell driver — generates 25 Hz AC via an H-bridge to ring the GPO bell
//
// Hardware: a boost converter raises 5 V to ~50 V DC.  An L293D (or similar)
// H-bridge alternates the polarity across the bell winding at 25 Hz.  The
// phone's internal 2 µF ringing capacitor (GPO 332) blocks DC and passes the
// AC to the bell coils.
//
// For the GPO 232 (no internal bell) the same output can drive an external
// Bellset No. 26 connected to the bell terminal.
//
// UK ring cadence:  400 ms ON — 200 ms OFF — 400 ms ON — 2000 ms OFF
// ============================================================================

class BellDriver {
public:
    void begin();

    // Start / stop the ring cadence.
    void startRinging();
    void stopRinging();

    // Single brief bell strike (boot-ready signal).
    void strike(unsigned long durationMs = 150);

    // Bell volume: 0 (silent) to 255 (full power). Controls H-bridge PWM duty.
    void setBellVolume(uint8_t vol);
    uint8_t bellVolume() const { return bell_volume_; }

    // Ring frequency in Hz (H-bridge toggle rate). UK exchanges used
    // 16-2/3 Hz (≈17) through 25 Hz depending on era. Clamped 10-50 Hz.
    void setRingFreq(int hz);
    int  ringFreq() const { return ring_freq_hz_; }

    // Must be called from loop().  Drives the cadence state machine and
    // the 25 Hz toggle.
    void update();

    bool isRinging() const { return ringing_; }

    // True when the cadence is in a silent (bell-off) step and has been for
    // at least guardMs. The 48 V bell drive couples onto the phone line and
    // pins the hook sense to "off-hook" while striking, so hook state can only
    // be trusted during these silent gaps. guardMs should be >= the hook
    // debounce so the debounced reading reflects the line, not stale coupling.
    bool inSilentGap(unsigned long guardMs) const;

private:
    void setBridgeOutput(bool phaseA);
    void setBridgeOff();

    bool          ringing_       = false;
    unsigned long cadence_start_ = 0;
    unsigned long toggle_time_   = 0;
    bool          phase_         = false;
    uint8_t       cadence_step_  = 0;
    uint8_t       bell_volume_   = 255;  // 0-255 PWM duty cycle
    int           ring_freq_hz_  = RING_FREQ_HZ;  // toggle rate (Hz)
};
