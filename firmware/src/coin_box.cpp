#include "coin_box.h"

void CoinBox::begin() {
    pinMode(PIN_COIN_SENSE, INPUT);
    pinMode(PIN_COIN_BTN_A, INPUT);
    pinMode(PIN_COIN_BTN_B, INPUT);

    // Auto-detect daughter board.  GPIO 36/39/35 are input-only pins with
    // no internal pull-up on the ESP32.  When the optocoupler daughter board
    // is connected, its pull-up resistors hold these pins HIGH, and the
    // optocoupler output pulls them LOW when active.  When nothing is
    // connected, the pins float — on the ESP32, unconnected input-only
    // pins typically read HIGH due to leakage, but we sample multiple
    // times to be sure.
    //
    // Detection strategy: if ANY pin reads LOW during the boot window, a
    // daughter board is present (an optocoupler is pulling a line down).
    // If all pins remain HIGH for the entire window, no board is installed.
    unsigned long start = millis();
    bool detected = false;
    while (millis() - start < COIN_DETECT_BOOT_MS) {
        if (digitalRead(PIN_COIN_SENSE) == LOW ||
            digitalRead(PIN_COIN_BTN_A) == LOW ||
            digitalRead(PIN_COIN_BTN_B) == LOW) {
            detected = true;
            break;
        }
        delay(10);
    }

    installed_ = detected;

    if (installed_) {
        Serial.println("[coin] A+B coin box daughter board DETECTED");
    } else {
        Serial.println("[coin] no coin box detected — feature disabled");
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
