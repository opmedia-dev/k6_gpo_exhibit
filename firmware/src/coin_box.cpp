#include "coin_box.h"

void CoinBox::begin() {
    pinMode(PIN_COIN_SENSE, INPUT);
    pinMode(PIN_COIN_BTN_A, INPUT);
    pinMode(PIN_COIN_BTN_B, INPUT);

    // Auto-detect daughter board.  GPIO 36/39/35 are input-only pins with
    // no internal pull-up on the ESP32.  External 10 kΩ pull-up resistors
    // (R4/R5/R6) on the carrier board hold these pins HIGH when no daughter
    // board is connected.  When the daughter board is present, its
    // optocoupler outputs can pull the pins LOW (overriding the pull-ups).
    //
    // Detection: require a majority of consecutive LOW samples to filter
    // out transient glitches from WiFi radio startup on GPIO 36/39.
    // A single LOW reading is not sufficient — need DETECT_THRESHOLD
    // consecutive readings with ANY pin LOW.
    static const int DETECT_THRESHOLD = 10;  // consecutive LOW samples needed
    unsigned long start = millis();
    int low_streak = 0;
    detected_ = false;
    while (millis() - start < COIN_DETECT_BOOT_MS) {
        if (digitalRead(PIN_COIN_SENSE) == LOW ||
            digitalRead(PIN_COIN_BTN_A) == LOW ||
            digitalRead(PIN_COIN_BTN_B) == LOW) {
            low_streak++;
            if (low_streak >= DETECT_THRESHOLD) {
                detected_ = true;
                break;
            }
        } else {
            low_streak = 0;
        }
        delay(10);
    }

    installed_ = (override_ == 1) || (override_ == -1 && detected_);

    if (detected_) {
        Serial.println("[coin] A+B coin box daughter board DETECTED");
    } else {
        Serial.println("[coin] no coin box detected");
    }
    if (override_ != -1) {
        Serial.printf("[coin] override: %s\n", override_ == 1 ? "FORCE ON" : "FORCE OFF");
    }
    if (installed_) {
        Serial.println("[coin] coin logic ACTIVE");
    } else {
        Serial.println("[coin] coin logic disabled");
    }
}

void CoinBox::update() {
    if (!installed_) return;

    // Read coin sense (active-low: LOW = coins inserted)
    bool coin_now = debounceRead(PIN_COIN_SENSE, coin_last_, coin_last_change_);
    coin_ready_ = coin_now;

    // Read Button A (active-low: LOW = pressed)
    bool a_now = debounceRead(PIN_COIN_BTN_A, btn_a_last_, btn_a_last_change_);
    if (a_now && !btn_a_state_) {
        btn_a_edge_ = true;  // rising edge
    }
    btn_a_state_ = a_now;

    // Read Button B (active-low: LOW = pressed)
    bool b_now = debounceRead(PIN_COIN_BTN_B, btn_b_last_, btn_b_last_change_);
    if (b_now && !btn_b_state_) {
        btn_b_edge_ = true;  // rising edge
    }
    btn_b_state_ = b_now;
}

bool CoinBox::coinsReady() const {
    if (!installed_) return true;  // pass-through when no coin box
    return coin_ready_;
}

bool CoinBox::buttonAPressed() const {
    if (!installed_) return true;  // pass-through when no coin box
    return btn_a_edge_;
}

bool CoinBox::buttonBPressed() const {
    if (!installed_) return false;
    return btn_b_edge_;
}

void CoinBox::setOverride(int mode) {
    override_ = mode;
    if (mode == 1)      installed_ = true;
    else if (mode == 0) installed_ = false;
    else                installed_ = detected_;
    Serial.printf("[coin] override set to %s — coin logic %s\n",
                  mode == 1 ? "FORCE ON" : mode == 0 ? "FORCE OFF" : "AUTO",
                  installed_ ? "ACTIVE" : "disabled");
}

bool CoinBox::debounceRead(int pin, bool& last, unsigned long& last_change) const {
    bool raw = (digitalRead(pin) == LOW);  // active-low → true when pressed/active
    if (raw != last) {
        last_change = millis();
        last = raw;
    }
    if (millis() - last_change >= COIN_DEBOUNCE_MS) {
        return last;
    }
    return !last;  // return previous stable state during debounce window
}
