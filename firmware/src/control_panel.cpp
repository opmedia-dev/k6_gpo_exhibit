#include "control_panel.h"
#include "config.h"

void ControlPanel::begin() {
    btns_[0] = { PIN_BTN_RING,   true, 0 };
    btns_[1] = { PIN_BTN_CANCEL, true, 0 };
    btns_[2] = { PIN_BTN_RESET,  true, 0 };
    btns_[3] = { PIN_BTN_MODE,   true, 0 };

    for (auto& b : btns_) {
        pinMode(b.pin, INPUT_PULLUP);
        b.last        = digitalRead(b.pin);
        b.last_change = millis();
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
        b.last_change = millis();
    }
    if (millis() - b.last_change >= BTN_DEBOUNCE_MS) {
        // Stable state — check for falling edge (HIGH→LOW = press).
        if (!current && b.last) {
            b.last = current;
            return true;
        }
        b.last = current;
    }
    return false;
}
