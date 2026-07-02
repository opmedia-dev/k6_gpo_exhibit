#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================================
// A+B Coin Box — optional daughter board interface
//
// Supports the classic GPO pre-payment coin mechanism (Mechanism No. 14 etc.)
// with three electrical contacts read via optocouplers on a daughter board:
//
//   COIN_SENSE  — coin weight switch, LOW when sufficient coins inserted
//   BTN_A       — Button A pressed (collect coins, connect speech)
//   BTN_B       — Button B pressed (refund coins)
//
// Auto-detection:  At boot, if all three inputs read HIGH (held by
// external 10 kΩ pull-ups R4/R5/R6 on the carrier board) for
// COIN_DETECT_BOOT_MS, the module assumes no daughter board is
// installed and disables itself.  All public methods then become
// no-ops and coinsReady()/buttonAPressed() return pass-through values
// so the phone controller operates normally without coin logic.
// ============================================================================

class CoinBox {
public:
    void begin();

    // Call every loop().  Reads and debounces the three coin inputs.
    void update();

    // True if the coin box is active (detected at boot or manually enabled).
    bool isInstalled() const { return installed_; }

    // Manual override — force coin box on or off regardless of detection.
    void setOverride(int mode);  // -1=auto, 0=force off, 1=force on
    int  overrideMode() const { return override_; }

    // True when sufficient coins are in the basket (weight switch closed).
    // Always returns true when no daughter board is installed.
    bool coinsReady() const;

    // True on the rising edge of Button A (collect coins).
    // Always returns true when no daughter board is installed.
    bool buttonAPressed() const;

    // True on the rising edge of Button B (refund coins).
    // Always returns false when no daughter board is installed.
    bool buttonBPressed() const;

    // Reset edge flags (call after consuming a press event).
    void clearButtonA() { btn_a_edge_ = false; }
    void clearButtonB() { btn_b_edge_ = false; }

private:
    bool debounceRead(int pin, bool& last, unsigned long& last_change) const;

    bool installed_ = false;
    bool detected_  = false;   // hardware detection result
    int  override_  = -1;      // -1=auto, 0=force off, 1=force on

    // Debounced states (active-low inputs, stored as logical state)
    bool coin_ready_  = false;
    bool btn_a_state_ = false;
    bool btn_b_state_ = false;

    // Edge detection
    bool btn_a_edge_ = false;
    bool btn_b_edge_ = false;

    // Debounce tracking
    bool          coin_last_ = false;
    unsigned long coin_last_change_ = 0;
    bool          btn_a_last_ = false;
    unsigned long btn_a_last_change_ = 0;
    bool          btn_b_last_ = false;
    unsigned long btn_b_last_change_ = 0;
};
