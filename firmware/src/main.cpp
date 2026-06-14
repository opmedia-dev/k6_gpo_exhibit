#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <esp_task_wdt.h>
#include "phone_controller.h"
#include "web_manager.h"
#include "logger.h"
#include "stats.h"
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
//   • Wi-Fi AP with web file manager and OTA firmware update
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
WebManager      web;
Logger          logger;
StatsTracker    stats;

// --- Safe mode (crash recovery) ---------------------------------------------
// RTC memory survives software resets but not power cycles.
RTC_DATA_ATTR static int  boot_crash_count = 0;
static bool safe_mode = false;
static const int  SAFE_MODE_THRESHOLD = 3;
static const unsigned long STABLE_BOOT_MS = 30000;  // 30s = considered stable

// --- Callbacks --------------------------------------------------------------

static void onDigit(uint8_t digit) {
    Serial.printf("[app] digit: %d\n", digit);
}

static void onNumber(const char* number) {
    Serial.printf("[app] number: %s\n", number);
    logger.callLog("DIAL number=%s", number);
}

static void onHook(HookState state) {
    Serial.printf("[app] hook: %s\n",
                  state == HookState::OFF_HOOK ? "OFF_HOOK" : "ON_HOOK");
}

static void onState(PhoneState state) {
    switch (state) {
    case PhoneState::RINGING:
        logger.callLog("INCOMING ring_start");
        stats.recordIncomingRing();
        break;
    case PhoneState::PLAYING_HISTORY:
        logger.callLog("INCOMING answered");
        stats.recordIncomingAnswered();
        stats.callStarted();
        break;
    case PhoneState::PLAYING_NUMBER:
        logger.callLog("OUTGOING connected number=%s", phone.dialledNumber());
        stats.recordOutgoingCall(phone.dialledNumber());
        stats.callStarted();
        break;
    case PhoneState::PLAYING_NOT_REC:
        logger.callLog("OUTGOING not_recognised number=%s", phone.dialledNumber());
        stats.recordNotRecognised(phone.dialledNumber());
        stats.callStarted();
        break;
    case PhoneState::IDLE:
        logger.callLog("IDLE");
        stats.callEnded();
        break;
    default:
        break;
    }
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

    // Safe mode: if the device has crashed SAFE_MODE_THRESHOLD times in a
    // row without running for STABLE_BOOT_MS, skip phone init and only
    // start Wi-Fi + web so the user can re-flash via OTA.
    boot_crash_count++;
    if (boot_crash_count >= SAFE_MODE_THRESHOLD) {
        safe_mode = true;
        Serial.println("*** SAFE MODE — phone logic disabled, web only ***");
        Serial.println("*** Upload new firmware via http://192.168.4.1/ ***");
        // Still need SD for the web file manager.
        SPI.begin();
        SD.begin(PIN_SD_CS);
        logger.begin();
        logger.systemLog("SAFE MODE entered after %d crashes", boot_crash_count);
        stats.begin();
        web.begin(logger, stats, phone);
        return;
    }

    phone.onDigit(onDigit);
    phone.onNumber(onNumber);
    phone.onHook(onHook);
    phone.onState(onState);
    phone.begin();
    logger.begin();
    stats.begin();

    Serial.println("[app] commands: R=ring  H=hangup  C=cancel  S=status  A=auto-ring  V0-9=vol");
    Serial.printf("[app] mode: %s (lamp %s)\n",
                  phone.autoRingEnabled() ? "AUTO" : "MANUAL",
                  phone.autoRingEnabled() ? "ON" : "OFF");
    if (phone.coinBox().isInstalled()) {
        Serial.println("[app] A+B coin box detected — coin logic active");
    }

    web.begin(logger, stats, phone);
    logger.systemLog("SD=%s coinbox=%s mode=%s",
                    phone.player().sdReady() ? "OK" : "FAIL",
                    phone.coinBox().isInstalled() ? "INSTALLED" : "NONE",
                    phone.autoRingEnabled() ? "AUTO" : "MANUAL");

    // Hardware watchdog: reboot if loop() stops for 15 seconds.
    esp_task_wdt_init(15, true);
    esp_task_wdt_add(NULL);
}

void loop() {
    esp_task_wdt_reset();

    // Once we've been running for STABLE_BOOT_MS, clear the crash counter.
    if (boot_crash_count > 0 && millis() > STABLE_BOOT_MS) {
        boot_crash_count = 0;
    }

    if (!safe_mode) {
        phone.update();
        phone.player().checkSdCard();
    }
    stats.update();
    web.update();
    handleSerial();
}
