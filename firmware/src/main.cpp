#include <Arduino.h>
#include "phone_controller.h"
#include "config.h"

// ============================================================================
// K6 GPO Exhibit — main application
//
// An ESP32 interface for an unmodified GPO 232/332 rotary telephone,
// designed for a K6 phone box exhibit.
//
// Features:
//   • Phone rings at random intervals; answering plays a history track
//   • Dialling a number plays a matching MP3 or "number not recognised"
//   • External control box: RING / CANCEL / RESET buttons
//   • All audio from SD card, played via I2S to MAX98357A DAC
//   • Optional A+B coin box (auto-detected daughter board on GPIO 36/39/35)
//
// Serial commands (115200 baud):
//   R   — trigger ring
//   H   — hang up / stop
//   C   — cancel ring
//   S   — print state
//   A   — toggle auto-ring on/off
//   V0-9 — set volume (0=min, 9=max)
// ============================================================================

PhoneController phone;

// --- Callbacks --------------------------------------------------------------

static void onDigit(uint8_t digit) {
    Serial.printf("[app] digit: %d\n", digit);
}

static void onNumber(const char* number) {
    Serial.printf("[app] number: %s\n", number);
}

static void onHook(HookState state) {
    Serial.printf("[app] hook: %s\n",
                  state == HookState::OFF_HOOK ? "OFF_HOOK" : "ON_HOOK");
}

static void onState(PhoneState state) {
    (void)state;
}

// --- Serial command handler -------------------------------------------------

static void handleSerial() {
    if (!Serial.available()) return;

    char c = Serial.read();
    switch (toupper(c)) {
    case 'R':
        Serial.println("[cmd] ring");
        phone.ring();
        break;
    case 'H':
        Serial.println("[cmd] hang up");
        phone.hangUp();
        break;
    case 'C':
        Serial.println("[cmd] cancel");
        phone.cancelRing();
        break;
    case 'S':
        Serial.printf("[cmd] state=%s  hook=%s  line=%d  mode=%s  sd=%s  coinbox=%s\n",
                      phone.stateName(),
                      phone.line().hookState() == HookState::OFF_HOOK
                          ? "OFF_HOOK" : "ON_HOOK",
                      phone.line().lastRawReading(),
                      phone.autoRingEnabled() ? "AUTO" : "MANUAL",
                      phone.player().sdReady() ? "OK" : "FAIL",
                      phone.coinBox().isInstalled() ? "INSTALLED" : "NONE");
        break;
    case 'A':
        phone.setAutoRing(!phone.autoRingEnabled());
        Serial.printf("[cmd] auto-ring %s\n",
                      phone.autoRingEnabled() ? "ON" : "OFF");
        break;
    case 'V': {
        // Read the next character as volume digit 0-9.
        unsigned long t = millis();
        while (!Serial.available() && millis() - t < 500) {}
        if (Serial.available()) {
            int v = Serial.read() - '0';
            if (v >= 0 && v <= 9) {
                uint8_t vol = map(v, 0, 9, 0, 21);
                phone.player().setVolume(vol);
                Serial.printf("[cmd] volume → %d/21\n", vol);
            }
        }
        break;
    }
    default:
        break;
    }
}

// --- Arduino entry points ---------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  K6 GPO Exhibit — ESP32 Phone Interface");
    Serial.println("  GPO 232 / 332 Rotary Telephone");
    Serial.println("========================================");
    Serial.println();

    phone.onDigit(onDigit);
    phone.onNumber(onNumber);
    phone.onHook(onHook);
    phone.onState(onState);
    phone.begin();

    Serial.println("[app] commands: R=ring  H=hangup  C=cancel  S=status  A=auto-ring  V0-9=vol");
    Serial.printf("[app] mode: %s (lamp %s)\n",
                  phone.autoRingEnabled() ? "AUTO" : "MANUAL",
                  phone.autoRingEnabled() ? "ON" : "OFF");
    if (phone.coinBox().isInstalled()) {
        Serial.println("[app] A+B coin box detected — coin logic active");
    }
}

void loop() {
    phone.update();
    handleSerial();
}
