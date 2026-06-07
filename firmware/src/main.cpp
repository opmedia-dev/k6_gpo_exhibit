#include <Arduino.h>
#include "phone_controller.h"
#include "config.h"

// ============================================================================
// K6 GPO Exhibit — main application
//
// Demonstrates a fully functional ESP32 interface to an unmodified GPO 232 or
// 332 rotary telephone over the original 3-core cord.
//
// Serial commands (115200 baud):
//   R         — trigger incoming ring
//   H         — hang up / stop ringing
//   S         — print current state
//   T<text>   — speak text (placeholder for future TTS)
//
// On handset lift the phone receives a UK dial tone.  Dialled digits are
// printed to Serial and passed to the registered callbacks.  After the
// number is complete the state transitions to CONNECTED, where bidirectional
// audio flows through the coupling transformer on the interface board.
// ============================================================================

PhoneController phone;

// --- Callbacks --------------------------------------------------------------

static void onDigit(uint8_t digit) {
    Serial.printf("[app] digit dialled: %d\n", digit);
}

static void onNumber(const char* number) {
    Serial.printf("[app] complete number: %s\n", number);
}

static void onHook(HookState state) {
    Serial.printf("[app] hook: %s\n",
                  state == HookState::OFF_HOOK ? "OFF_HOOK" : "ON_HOOK");
}

// --- Serial command handler -------------------------------------------------

static void handleSerial() {
    if (!Serial.available()) return;

    char c = Serial.read();
    switch (toupper(c)) {
    case 'R':
        Serial.println("[app] → ring command");
        phone.ring();
        break;
    case 'H':
        Serial.println("[app] → hang up command");
        phone.hangUp();
        break;
    case 'S':
        Serial.printf("[app] state=%d  hook=%s  line_raw=%d\n",
                      (int)phone.state(),
                      phone.line().hookState() == HookState::OFF_HOOK
                          ? "OFF_HOOK" : "ON_HOOK",
                      phone.line().lastRawReading());
        break;
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
    phone.begin();

    Serial.println("[app] ready — send 'R' to ring, 'H' to hang up, 'S' for status");
}

void loop() {
    phone.update();
    handleSerial();
}
