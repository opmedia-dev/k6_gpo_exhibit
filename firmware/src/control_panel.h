#pragma once

#include <Arduino.h>

// ============================================================================
// External control panel — three buttons on the operator's box
//
// All buttons are active-low with the ESP32 internal pull-up enabled.
// Pressing a button connects the pin to GND.
//
//   BTN_RING   — manually trigger the phone bell
//   BTN_CANCEL — cancel ringing / stop current playback
//   BTN_RESET  — full system restart
// ============================================================================

enum class Button : uint8_t {
    RING,
    CANCEL,
    RESET,
    NONE
};

class ControlPanel {
public:
    void begin();

    // Call every loop().  Returns the button that was just pressed (falling
    // edge after debounce), or Button::NONE.
    Button update();

private:
    struct BtnState {
        int           pin;
        bool          last;
        unsigned long last_change;
    };

    BtnState btns_[3] = {};

    bool debounceRead(BtnState& b);
};
