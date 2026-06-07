#include "phone_controller.h"
#include "config.h"

// Timeout (ms) after last digit before the number is considered complete.
static constexpr unsigned long NUMBER_COMPLETE_TIMEOUT_MS = 3000;

// Maximum time in DIAL_TONE before reverting to busy (exchange timeout).
static constexpr unsigned long DIAL_TONE_TIMEOUT_MS = 15000;

void PhoneController::begin() {
    line_.begin();
    dial_.begin();
    bell_.begin();
    audio_.begin();

    pinMode(PIN_STATUS_LED, OUTPUT);
    enterState(PhoneState::IDLE);

    Serial.println("[phone] controller ready");
}

void PhoneController::update() {
    // Always poll the line — this gives us hook state and raw ADC.
    line_.update();

    // Notify on hook change.
    if (line_.hookChanged() && hook_cb_) {
        hook_cb_(line_.hookState());
    }

    // Bell cadence runs independently.
    bell_.update();

    switch (state_) {

    // ----- IDLE ----------------------------------------------------------
    case PhoneState::IDLE:
        if (line_.hookState() == HookState::OFF_HOOK && line_.hookChanged()) {
            enterState(PhoneState::DIAL_TONE);
        }
        break;

    // ----- RINGING -------------------------------------------------------
    case PhoneState::RINGING:
        if (line_.hookState() == HookState::OFF_HOOK && line_.hookChanged()) {
            bell_.stopRinging();
            enterState(PhoneState::CONNECTED);
        }
        break;

    // ----- DIAL TONE -----------------------------------------------------
    case PhoneState::DIAL_TONE:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            audio_.stopTone();
            enterState(PhoneState::IDLE);
            break;
        }
        // Check for first dial pulse — transition to DIALING.
        if (dial_.update(line_.isLineBreak())) {
            audio_.stopTone();
            uint8_t d = dial_.digit();
            if (dial_pos_ < MAX_DIALLED_DIGITS) {
                dialled_[dial_pos_++] = '0' + d;
                dialled_[dial_pos_]   = '\0';
            }
            last_digit_time_ = millis();
            if (digit_cb_) digit_cb_(d);
            Serial.printf("[phone] digit: %d  number so far: %s\n", d, dialled_);
            enterState(PhoneState::DIALING);
            break;
        }
        // If the line break starts, we may be about to dial — keep listening.
        if (line_.isLineBreak()) {
            audio_.stopTone();
        }
        // Timeout → busy tone.
        if (millis() - state_enter_time_ > DIAL_TONE_TIMEOUT_MS) {
            audio_.stopTone();
            enterState(PhoneState::BUSY);
        }
        break;

    // ----- DIALING -------------------------------------------------------
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
            Serial.printf("[phone] digit: %d  number so far: %s\n", d, dialled_);
        }
        // Number complete after timeout.
        if (dial_pos_ > 0 &&
            millis() - last_digit_time_ > NUMBER_COMPLETE_TIMEOUT_MS) {
            Serial.printf("[phone] number complete: %s\n", dialled_);
            if (number_cb_) number_cb_(dialled_);
            enterState(PhoneState::CONNECTED);
        }
        break;

    // ----- CONNECTED -----------------------------------------------------
    case PhoneState::CONNECTED:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            enterState(PhoneState::IDLE);
        }
        // Application can read audio via audio_.readSample() and inject via
        // audio_.writeSample() from external code.
        break;

    // ----- BUSY ----------------------------------------------------------
    case PhoneState::BUSY:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            audio_.stopTone();
            enterState(PhoneState::IDLE);
        }
        break;
    }

    // Status LED: on when off-hook, blink when ringing.
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

void PhoneController::hangUp() {
    bell_.stopRinging();
    audio_.stopTone();
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
        Serial.println("[phone] → IDLE");
        break;
    case PhoneState::RINGING:
        Serial.println("[phone] → RINGING");
        break;
    case PhoneState::DIAL_TONE:
        dial_.reset();
        dial_pos_   = 0;
        dialled_[0] = '\0';
        Serial.println("[phone] → DIAL_TONE");
        audio_.playDialTone(DIAL_TONE_TIMEOUT_MS);
        break;
    case PhoneState::DIALING:
        Serial.println("[phone] → DIALING");
        break;
    case PhoneState::CONNECTED:
        Serial.println("[phone] → CONNECTED");
        break;
    case PhoneState::BUSY:
        Serial.println("[phone] → BUSY");
        audio_.playBusyTone(30);
        break;
    }
}
