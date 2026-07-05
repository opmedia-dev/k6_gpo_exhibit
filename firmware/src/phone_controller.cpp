#include "phone_controller.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>

void PhoneController::begin() {
    line_.begin();
    dial_.begin();
    bell_.begin();
    panel_.begin();
    coin_box_.begin();

    bool sdOk = player_.begin();

    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_AUTO_LAMP, OUTPUT);
    updateLamp();

    randomSeed(analogRead(0) ^ micros());
    last_activity_ms_ = millis();
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
    } else if (btn == Button::MODE) {
        toggleAutoRing();
        Serial.printf("[panel] MODE pressed — auto-ring %s\n",
                      auto_ring_enabled_ ? "ON" : "OFF");
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
        // Handset lifted — always go to dial tone (no coin gate here).
        if (line_.hookState() == HookState::OFF_HOOK && line_.hookChanged()) {
            enterState(PhoneState::DIAL_TONE);
        }
        break;

    // ----- RINGING -----------------------------------------------------------
    case PhoneState::RINGING:
        // Answering an incoming ring — no A+B interaction required.
        if (line_.hookState() == HookState::OFF_HOOK && line_.hookChanged()) {
            bell_.stopRinging();
            enterState(PhoneState::PLAYING_HISTORY);
            break;
        }
        // Fixed-duration test ring — stop after the requested time.
        if (test_ring_end_ && millis() >= test_ring_end_) {
            test_ring_end_ = 0;
            bell_.stopRinging();
            enterState(PhoneState::IDLE);
            Serial.println("[phone] test ring finished");
            break;
        }
        // Ring count limit: one cadence cycle ≈ 3s.  Skipped during test ring.
        if (!test_ring_end_ && max_ring_cadences_ > 0) {
            unsigned long elapsed = millis() - state_enter_time_;
            int cadences = elapsed / 3000;
            if (cadences >= max_ring_cadences_) {
                Serial.println("[phone] ring count limit reached");
                bell_.stopRinging();
                enterState(PhoneState::IDLE);
            }
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
            if (!replace_prompted_ && player_.sdReady() && SD.exists(SD_FILE_REPLACE)) {
                replace_prompted_ = true;
                player_.playFile(SD_FILE_REPLACE, false);
            } else {
                enterState(PhoneState::BUSY);
            }
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

            // Check for a plugin script first.
            if (tryPlugin(dialled_)) break;

            // Build path (resolving aliases) and check if number is recognised.
            String resolved = player_.resolveAlias(dialled_);
            snprintf(pending_path_, sizeof(pending_path_),
                     "%s/%s.mp3", SD_DIR_NUMBERS, resolved.c_str());

            bool recognised = player_.sdReady() && SD.exists(pending_path_);

            if (coin_box_.isInstalled()) {
                if (recognised) {
                    // Recognised — need coins then Button A before audio.
                    if (coin_box_.coinsReady()) {
                        // Coins already in — wait for Button A.
                        Serial.println("[coin] coins already in — press A to connect");
                        enterState(PhoneState::AWAIT_BTN_A);
                    } else {
                        enterState(PhoneState::AWAIT_COINS);
                    }
                } else {
                    // Not recognised — play announcement, prompt B for refund.
                    player_.playNotRecognised();
                    enterState(PhoneState::AWAIT_BTN_B);
                }
            } else {
                // No coin box — play directly.
                if (recognised) {
                    // Play ringing tone first if available.
                    if (player_.sdReady() && SD.exists(SD_FILE_RING_TONE)) {
                        enterState(PhoneState::RINGING_TONE);
                    } else {
                        player_.playFile(pending_path_, false);
                        enterState(PhoneState::PLAYING_NUMBER);
                    }
                } else {
                    player_.playNotRecognised();
                    enterState(PhoneState::PLAYING_NOT_REC);
                }
            }
        }
        break;

    // ----- AWAIT_COINS (A+B only) -------------------------------------------
    // Number was recognised; waiting for coins to be inserted.
    case PhoneState::AWAIT_COINS:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (coin_box_.coinsReady()) {
            player_.stop();
            Serial.println("[coin] coins accepted — press A to connect");
            enterState(PhoneState::AWAIT_BTN_A);
        }
        break;

    // ----- AWAIT_BTN_A (A+B only) -------------------------------------------
    // Coins are in.  Waiting for Button A to collect coins and connect call.
    case PhoneState::AWAIT_BTN_A:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (coin_box_.buttonAPressed()) {
            coin_box_.clearButtonA();
            Serial.println("[coin] Button A — coins collected, connecting");
            player_.playFile(pending_path_, false);
            enterState(PhoneState::PLAYING_NUMBER);
            break;
        }
        if (coin_box_.buttonBPressed()) {
            coin_box_.clearButtonB();
            Serial.println("[coin] Button B — coins refunded");
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (millis() - state_enter_time_ > COIN_BTN_A_TIMEOUT_MS) {
            Serial.println("[coin] Button A timeout — busy");
            player_.stop();
            enterState(PhoneState::BUSY);
        }
        break;

    // ----- AWAIT_BTN_B (A+B only) -------------------------------------------
    // Number not recognised.  Press B to refund coins and hang up.
    case PhoneState::AWAIT_BTN_B:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (coin_box_.buttonBPressed()) {
            coin_box_.clearButtonB();
            Serial.println("[coin] Button B — coins refunded");
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (!player_.isPlaying()) {
            // "Not recognised" finished — play "press B" prompt or go to busy.
            if (player_.sdReady() && SD.exists(SD_FILE_PRESS_B)) {
                player_.playFile(SD_FILE_PRESS_B, true);
            } else {
                enterState(PhoneState::BUSY);
            }
        }
        break;

    // ----- RINGING_TONE (outgoing call) --------------------------------------
    case PhoneState::RINGING_TONE:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        if (millis() - state_enter_time_ >= ring_tone_target_) {
            player_.stop();
            player_.playFile(pending_path_, false);
            enterState(PhoneState::PLAYING_NUMBER);
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
            if (!replace_prompted_ && player_.sdReady() && SD.exists(SD_FILE_REPLACE)) {
                replace_prompted_ = true;
                player_.playFile(SD_FILE_REPLACE, false);
            } else {
                enterState(PhoneState::BUSY);
            }
        }
        break;

    // ----- PLUGIN_SEQUENCE ----------------------------------------------------
    case PhoneState::PLUGIN_SEQUENCE:
        if (line_.hookState() == HookState::ON_HOOK && line_.hookChanged()) {
            player_.stop();
            enterState(PhoneState::IDLE);
            break;
        }
        // Advance through plugin steps.
        if (plugin_index_ < plugin_count_) {
            auto& step = plugin_steps_[plugin_index_];
            if (step.action == 'P') {
                // Play: start file, advance when it finishes.
                if (!player_.isPlaying() && plugin_step_time_ == 0) {
                    player_.playFile(step.path, false);
                    plugin_step_time_ = millis();
                } else if (!player_.isPlaying() && plugin_step_time_ > 0) {
                    plugin_index_++;
                    plugin_step_time_ = 0;
                }
            } else if (step.action == 'D') {
                // Delay: wait ms then advance.
                if (plugin_step_time_ == 0) {
                    plugin_step_time_ = millis();
                } else if (millis() - plugin_step_time_ >= step.ms) {
                    plugin_index_++;
                    plugin_step_time_ = 0;
                }
            } else if (step.action == 'L') {
                // Loop: play file in a loop, advance after ms.
                if (plugin_step_time_ == 0) {
                    player_.playFile(step.path, true);
                    plugin_step_time_ = millis();
                } else if (millis() - plugin_step_time_ >= step.ms) {
                    player_.stop();
                    plugin_index_++;
                    plugin_step_time_ = 0;
                }
            } else {
                plugin_index_++;  // unknown action, skip
            }
        } else {
            // All steps done — replace handset prompt or busy.
            if (!replace_prompted_ && player_.sdReady() && SD.exists(SD_FILE_REPLACE)) {
                replace_prompted_ = true;
                player_.playFile(SD_FILE_REPLACE, false);
                enterState(PhoneState::PLAYING_NUMBER);  // reuse for hangup handling
            } else {
                enterState(PhoneState::BUSY);
            }
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

    // --- Auto-mode lamp (update continuously for flash pattern) -------------
    updateLamp();
}

void PhoneController::ring() {
    if (state_ != PhoneState::IDLE) return;
    test_ring_end_ = 0;
    bell_.startRinging();
    enterState(PhoneState::RINGING);
    Serial.println("[phone] ringing");
}

void PhoneController::testRing(int seconds) {
    if (state_ != PhoneState::IDLE) return;
    if (seconds < 1) seconds = 1;
    if (seconds > 30) seconds = 30;
    test_ring_end_ = millis() + (unsigned long)seconds * 1000;
    bell_.startRinging();
    enterState(PhoneState::RINGING);
    Serial.printf("[phone] test ring for %ds\n", seconds);
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
        // Play "press Button A" prompt if available.
        if (player_.sdReady() && SD.exists(SD_FILE_PRESS_A)) {
            player_.playFile(SD_FILE_PRESS_A, true);
        }
        break;

    case PhoneState::AWAIT_BTN_B:
        // "Not recognised" is already playing from the DIALING state transition.
        // After it finishes, play "press Button B" prompt if available.
        break;

    case PhoneState::PLAYING_HISTORY:
        replace_prompted_ = false;
        last_activity_ms_ = millis();
        player_.playRandomHistory();
        break;

    case PhoneState::DIAL_TONE:
        last_activity_ms_ = millis();
        dial_.reset();
        dial_pos_   = 0;
        dialled_[0] = '\0';
        player_.playDialTone();
        break;

    case PhoneState::DIALING:
        break;

    case PhoneState::RINGING_TONE:
        ring_tone_target_ = random(ring_tone_min_ms_, ring_tone_max_ms_ + 1);
        player_.playFile(SD_FILE_RING_TONE, true);  // loop ringing tone
        break;

    case PhoneState::PLAYING_NUMBER:
        replace_prompted_ = false;
        break;

    case PhoneState::PLAYING_NOT_REC:
        replace_prompted_ = false;
        break;

    case PhoneState::PLUGIN_SEQUENCE:
        replace_prompted_ = false;
        last_activity_ms_ = millis();
        plugin_index_ = 0;
        plugin_step_time_ = 0;
        break;

    case PhoneState::BUSY:
        player_.playBusyTone();
        break;
    }

    Serial.printf("[phone] → %s\n", stateName());
    if (state_cb_) state_cb_(s);
}

void PhoneController::setAutoRing(bool enabled) {
    auto_ring_enabled_ = enabled;
    updateLamp();
    if (enabled) resetAutoRingTimer();
}

void PhoneController::updateLamp() {
    if (isAlertActive()) {
        // Flash lamp: 500ms on, 500ms off warning pattern.
        digitalWrite(PIN_AUTO_LAMP, (millis() / 500) % 2 ? HIGH : LOW);
    } else {
        digitalWrite(PIN_AUTO_LAMP, auto_ring_enabled_ ? HIGH : LOW);
    }
}

bool PhoneController::isAlertActive() const {
    if (alert_idle_ms_ == 0) return false;
    if (state_ != PhoneState::IDLE) return false;
    return (millis() - last_activity_ms_) > alert_idle_ms_;
}

void PhoneController::setAutoRingInterval(unsigned long minMs, unsigned long maxMs) {
    auto_ring_min_ms_ = minMs;
    auto_ring_max_ms_ = maxMs;
    if (auto_ring_enabled_) resetAutoRingTimer();
}

void PhoneController::setRingToneRange(int minSecs, int maxSecs) {
    if (minSecs < 2) minSecs = 2;
    if (maxSecs < minSecs) maxSecs = minSecs;
    if (maxSecs > 15) maxSecs = 15;
    ring_tone_min_ms_ = minSecs * 1000UL;
    ring_tone_max_ms_ = maxSecs * 1000UL;
}

void PhoneController::resetAutoRingTimer() {
    unsigned long interval = random(auto_ring_min_ms_, auto_ring_max_ms_);
    next_ring_time_ = millis() + interval;
    Serial.printf("[phone] next auto-ring in %lu s\n", interval / 1000);
}

bool PhoneController::tryPlugin(const char* number) {
    if (!player_.sdReady()) return false;

    char path[48];
    snprintf(path, sizeof(path), "%s/%s.json", SD_DIR_PLUGINS, number);
    if (!SD.exists(path)) return false;

    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();

    JsonArray steps = doc["steps"].as<JsonArray>();
    if (steps.isNull() || steps.size() == 0) return false;

    plugin_count_ = 0;
    for (JsonObject step : steps) {
        if (plugin_count_ >= MAX_PLUGIN_STEPS) break;
        auto& ps = plugin_steps_[plugin_count_];
        const char* action = step["action"] | "";
        if (strcmp(action, "play") == 0) {
            ps.action = 'P';
            strlcpy(ps.path, step["file"] | "", sizeof(ps.path));
            ps.ms = 0;
        } else if (strcmp(action, "delay") == 0) {
            ps.action = 'D';
            ps.path[0] = '\0';
            ps.ms = step["ms"] | 1000;
        } else if (strcmp(action, "loop") == 0) {
            ps.action = 'L';
            strlcpy(ps.path, step["file"] | "", sizeof(ps.path));
            ps.ms = step["ms"] | 5000;
        } else {
            continue;
        }
        plugin_count_++;
    }

    if (plugin_count_ == 0) return false;

    Serial.printf("[phone] plugin loaded: %s (%d steps)\n", path, plugin_count_);
    enterState(PhoneState::PLUGIN_SEQUENCE);
    return true;
}

const char* PhoneController::stateName() const {
    switch (state_) {
    case PhoneState::IDLE:            return "IDLE";
    case PhoneState::RINGING:         return "RINGING";
    case PhoneState::PLAYING_HISTORY: return "PLAYING_HISTORY";
    case PhoneState::DIAL_TONE:       return "DIAL_TONE";
    case PhoneState::DIALING:         return "DIALING";
    case PhoneState::AWAIT_COINS:     return "AWAIT_COINS";
    case PhoneState::AWAIT_BTN_A:     return "AWAIT_BTN_A";
    case PhoneState::AWAIT_BTN_B:     return "AWAIT_BTN_B";
    case PhoneState::RINGING_TONE:    return "RINGING_TONE";
    case PhoneState::PLAYING_NUMBER:  return "PLAYING_NUMBER";
    case PhoneState::PLAYING_NOT_REC: return "PLAYING_NOT_REC";
    case PhoneState::PLUGIN_SEQUENCE: return "PLUGIN_SEQUENCE";
    case PhoneState::BUSY:            return "BUSY";
    default:                          return "UNKNOWN";
    }
}
