#pragma once

#include <Arduino.h>

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

    // Must be called from loop().  Drives the cadence state machine and
    // the 25 Hz toggle.
    void update();

    bool isRinging() const { return ringing_; }

private:
    void setBridgeOutput(bool phaseA);
    void setBridgeOff();

    bool          ringing_       = false;
    unsigned long cadence_start_ = 0;
    unsigned long toggle_time_   = 0;
    bool          phase_         = false;
    uint8_t       cadence_step_  = 0;
};
