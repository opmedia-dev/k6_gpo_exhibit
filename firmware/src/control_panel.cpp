#include "control_panel.h"
#include "config.h"

void ControlPanel::begin() {
    btns_[0] = { PIN_BTN_RING,   true, true, 0 };
    btns_[1] = { PIN_BTN_CANCEL, true, true, 0 };
    btns_[2] = { PIN_BTN_RESET,  true, true, 0 };
    btns_[3] = { PIN_BTN_MODE,   true, true, 0 };

    for (auto& b : btns_) {
        pinMode(b.pin, INPUT_PULLUP);
        bool reading   = digitalRead(b.pin);
        b.last         = reading;
        b.stable       = reading;
        b.last_change  = millis();
    }
}

Button ControlPanel::update() {
    for (int i = 0; i < 4; i++) {
        if (debounceRead(btns_[i])) {
            return static_cast<Button>(i);
        }
    }
    return Button::NONE;
}

// Returns true on a debounced falling edge (button press).
bool ControlPanel::debounceRead(BtnState& b) {
    bool current = digitalRead(b.pin);
    if (current != b.last) {
        b.last        = current;
        b.last_change = millis();
    }
    if (millis() - b.last_change >= BTN_DEBOUNCE_MS) {
        if (current != b.stable) {
            bool was_high = b.stable;
            b.stable = current;
            if (!current && was_high) return true;  // falling edge = press
        }
    }
    return false;
}
