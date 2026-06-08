#include "phone_controller.h"
#include "config.h"

void PhoneController::begin() {
    line_.begin();
    dial_.begin();
    bell_.begin();
    panel_.begin();
    coin_box_.begin();

    bool sdOk = player_.begin();

    pinMode(PIN_STATUS_LED, OUTPUT);
    randomSeed(analogRead(0) ^ micros());
    resetAutoRingTimer();
    enterState(PhoneState::IDLE);

    Serial.println("[phone] controller ready");
    if (!sdOk) Serial.println("[phone] WARNING: SD card not available — no audio playback");
}

void PhoneController::update() {
    // --- poll inputs --------------------------------------------------------
    line_.update();
    bell_.update();
    player_.update();
    coin_box_.update();

    if (line_.hookChanged() && hook_cb_) {
        hook_cb_(line_.hookState());
    }

    // --- control panel buttons ----------------------------------------------
    Button btn = panel_.update();
    if (btn == Button::RING) {
        Serial.println("[panel] RING pressed");
        ring();
    } else if (btn == Button::CANCEL) {
        Serial.println("[panel] CANCEL pressed");
        cancelRing();
    } else if (btn == Button::RESET) {
        Serial.println("[panel] RESET pressed — rebooting");
        delay(200);
        ESP.restart();
    }

    // --- state machine ------------------------------------------------------
    switch (state_) {

    // ----- IDLE --------------------------------------------------------------
    case PhoneState::IDLE:
        // Auto-ring timer.
        if (auto_ring_enabled_ && millis() >= next_ring_time_) {
            Serial.println("[phone] auto-ring timer fired");
            ring();
            break;
        }
        // Handset lifted.
        if (line_.hookState() == HookState::OFF_HOOK && line_.hookChanged()) {
            if (coin_box_.isInstalled() && !coin_box_.coinsReady()) {
                enterState(PhoneState::AWAIT_COINS);
            } else {
                enterState(PhoneState::DIAL_TONE);
            }
        }
        break;

    // ----- RINGING -----------------------------------------------------------
    case PhoneState::RINGING:
        if (line_.hookState() == HookState::OFF_HOOK && line_.hookChanged()) {
            bell_.stopRinging();
            if (coin_box_.isInstalled()) {
                enterState(PhoneState::AWAIT_BTN_A);
            } else {
                enterState(PhoneState::PLAYING_HISTORY);
            }
        }
        break;

    // ----- AWAIT_COINS (A+B only) -------------------------------------------
    case PhoneState::AWAIT_COINS:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (coin_box_.coinsReady()) {
            player_.stop();
            Serial.println("[coin] coins accepted — dial tone");
            enterState(PhoneState::DIAL_TONE);
        }
        break;

    // ----- AWAIT_BTN_A (A+B only) -------------------------------------------
    case PhoneState::AWAIT_BTN_A:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (coin_box_.buttonAPressed()) {
            coin_box_.clearButtonA();
            Serial.println("[coin] Button A — coins collected, connecting");
            enterState(PhoneState::PLAYING_HISTORY);
            break;
        }
        if (coin_box_.buttonBPressed()) {
            coin_box_.clearButtonB();
            Serial.println("[coin] Button B — coins refunded");
            enterState(PhoneState::IDLE);
            break;
        }
        if (millis() - state_enter_time_ > COIN_BTN_A_TIMEOUT_MS) {
            Serial.println("[coin] Button A timeout");
            enterState(PhoneState::BUSY);
        }
        break;

    // ----- PLAYING_HISTORY ---------------------------------------------------
    case PhoneState::PLAYING_HISTORY:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (!player_.isPlaying()) {
            // Track finished — return to idle.
            enterState(PhoneState::IDLE);
        }
        break;

    // ----- DIAL TONE ---------------------------------------------------------
    case PhoneState::DIAL_TONE:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (dial_.update(line_.isLineBreak())) {
            player_.stop();
            uint8_t d = dial_.digit();
            if (dial_pos_ < MAX_DIALLED_DIGITS) {
                dialled_[dial_pos_++] = '0' + d;
                dialled_[dial_pos_]   = '\0';
            }
            last_digit_time_ = millis();
            if (digit_cb_) digit_cb_(d);
            Serial.printf("[phone] digit: %d  number: %s\n", d, dialled_);
            enterState(PhoneState::DIALING);
            break;
        }
        if (line_.isLineBreak()) {
            player_.stop();
        }
        if (millis() - state_enter_time_ > DIAL_TONE_TIMEOUT_MS) {
            player_.stop();
            enterState(PhoneState::BUSY);
        }
        break;

    // ----- DIALING -----------------------------------------------------------
    case PhoneState::DIALING:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            dial_.reset();
            enterState(PhoneState::IDLE);
            break;
        }
        if (dial_.update(line_.isLineBreak())) {
            uint8_t d = dial_.digit();
            if (dial_pos_ < MAX_DIALLED_DIGITS) {
                dialled_[dial_pos_++] = '0' + d;
                dialled_[dial_pos_]   = '\0';
            }
            last_digit_time_ = millis();
            if (digit_cb_) digit_cb_(d);
            Serial.printf("[phone] digit: %d  number: %s\n", d, dialled_);
        }
        // Number complete after inter-digit timeout.
        if (dial_pos_ > 0 &&
            millis() - last_digit_time_ > NUMBER_COMPLETE_MS) {
            Serial.printf("[phone] number complete: %s\n", dialled_);
            if (number_cb_) number_cb_(dialled_);
            // Try to play the matching track.
            char path[64];
            snprintf(path, sizeof(path), "%s/%s.mp3", SD_DIR_NUMBERS, dialled_);
            if (player_.sdReady() && SD.exists(path)) {
                player_.playFile(path, false);
                enterState(PhoneState::PLAYING_NUMBER);
            } else {
                player_.playNotRecognised();
                enterState(PhoneState::PLAYING_NOT_REC);
            }
        }
        break;

    // ----- PLAYING_NUMBER / PLAYING_NOT_REC ----------------------------------
    case PhoneState::PLAYING_NUMBER:
    case PhoneState::PLAYING_NOT_REC:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (!player_.isPlaying()) {
            // Playback finished — go to busy tone so user hangs up.
            enterState(PhoneState::BUSY);
        }
        break;

    // ----- BUSY --------------------------------------------------------------
    case PhoneState::BUSY:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
        }
        break;
    }

    // --- Status LED ----------------------------------------------------------
    if (state_ == PhoneState::RINGING) {
        digitalWrite(PIN_STATUS_LED, (millis() / 250) % 2);
    } else {
        digitalWrite(PIN_STATUS_LED,
                     line_.hookState() == HookState::OFF_HOOK ? HIGH : LOW);
    }
}

void PhoneController::ring() {
    if (state_ != PhoneState::IDLE) return;
    bell_.startRinging();
    enterState(PhoneState::RINGING);
    Serial.println("[phone] ringing");
}

void PhoneController::cancelRing() {
    if (state_ == PhoneState::RINGING) {
        bell_.stopRinging();
        enterState(PhoneState::IDLE);
        Serial.println("[phone] ring cancelled");
    } else {
        // Cancel also stops any current playback.
        player_.stop();
        if (state_ != PhoneState::IDLE) {
            enterState(PhoneState::IDLE);
        }
    }
}

void PhoneController::hangUp() {
    bell_.stopRinging();
    player_.stop();
    if (state_ != PhoneState::IDLE) {
        enterState(PhoneState::IDLE);
    }
}

void PhoneController::enterState(PhoneState s) {
    state_            = s;
    state_enter_time_ = millis();

    switch (s) {
    case PhoneState::IDLE:
        dial_.reset();
        dial_pos_   = 0;
        dialled_[0] = '\0';
        resetAutoRingTimer();
        break;

    case PhoneState::RINGING:
        break;

    case PhoneState::AWAIT_COINS:
        // Play "insert coins" prompt if available, otherwise silence.
        if (player_.sdReady() && SD.exists(SD_FILE_INSERT)) {
            player_.playFile(SD_FILE_INSERT, true);
        }
        break;

    case PhoneState::AWAIT_BTN_A:
        break;

    case PhoneState::PLAYING_HISTORY:
        player_.playRandomHistory();
        break;

    case PhoneState::DIAL_TONE:
        dial_.reset();
        dial_pos_   = 0;
        dialled_[0] = '\0';
        player_.playDialTone();
        break;

    case PhoneState::DIALING:
        break;

    case PhoneState::PLAYING_NUMBER:
        break;

    case PhoneState::PLAYING_NOT_REC:
        break;

    case PhoneState::BUSY:
        player_.playBusyTone();
        break;
    }

    Serial.printf("[phone] → %s\n", stateName());
    if (state_cb_) state_cb_(s);
}

void PhoneController::resetAutoRingTimer() {
    unsigned long interval = random(AUTO_RING_MIN_MS, AUTO_RING_MAX_MS);
    next_ring_time_ = millis() + interval;
    Serial.printf("[phone] next auto-ring in %lu s\n", interval / 1000);
}

const char* PhoneController::stateName() const {
    switch (state_) {
    case PhoneState::IDLE:            return "IDLE";
    case PhoneState::RINGING:         return "RINGING";
    case PhoneState::AWAIT_COINS:     return "AWAIT_COINS";
    case PhoneState::PLAYING_HISTORY: return "PLAYING_HISTORY";
    case PhoneState::AWAIT_BTN_A:     return "AWAIT_BTN_A";
    case PhoneState::DIAL_TONE:       return "DIAL_TONE";
    case PhoneState::DIALING:         return "DIALING";
    case PhoneState::PLAYING_NUMBER:  return "PLAYING_NUMBER";
    case PhoneState::PLAYING_NOT_REC: return "PLAYING_NOT_REC";
    case PhoneState::BUSY:            return "BUSY";
    default:                          return "UNKNOWN";
    }
}
