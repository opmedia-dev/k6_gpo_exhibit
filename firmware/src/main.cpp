#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
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
//   B   — cycle coin box override (auto → off → on → auto)
//   T   — bring-up self-test checklist
//   K   — line-sense calibration wizard
//   E   — toggle rotary self-confirm (echo digits on lamp)
//   I   — toggle earpiece dial-pulse clicks
//   Q   — audio probe (1 kHz peak/RMS of digital feed)
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

// Debug flag in audio_player.cpp: when true, the I2S hook logs call rate + peak.
extern volatile bool g_audio_hook_debug;

// Debug flag in phone_line.cpp: when true, logs raw line reading + dial edges.
extern volatile bool g_line_debug;

// Rotary self-confirm mode: when true, each decoded digit is echoed as lamp
// blinks so dialling can be verified on the bench with no laptop attached.
volatile bool g_dial_confirm = false;

// Non-blocking lamp echo for rotary self-confirm mode. Digits are queued and
// blinked out by serviceDialEcho() from the main loop so dialling and the state
// machine are never stalled (0 is shown as 10 blinks).
static uint8_t       s_echo_queue[16];
static uint8_t       s_echo_head = 0;
static uint8_t       s_echo_tail = 0;
static int           s_echo_blinks_left = 0;
static uint8_t       s_echo_phase = 0;   // 0 idle,1 pre-gap,2 on,3 off,4 done
static unsigned long s_echo_next  = 0;

static void queueDigitEcho(uint8_t digit) {
    uint8_t next = (uint8_t)((s_echo_tail + 1) % sizeof(s_echo_queue));
    if (next == s_echo_head) return;  // queue full: drop
    s_echo_queue[s_echo_tail] = (digit == 0) ? 10 : digit;
    s_echo_tail = next;
}

static void serviceDialEcho() {
    unsigned long now = millis();
    if (s_echo_phase == 0) {
        if (s_echo_head == s_echo_tail) return;  // nothing queued
        s_echo_blinks_left = s_echo_queue[s_echo_head];
        s_echo_head = (uint8_t)((s_echo_head + 1) % sizeof(s_echo_queue));
        s_echo_phase = 1;
        s_echo_next  = now + 250;  // short gap so separate digits are distinct
        return;
    }
    if (now < s_echo_next) return;
    switch (s_echo_phase) {
        case 1:  // pre-gap done -> lamp on
            digitalWrite(PIN_AUTO_LAMP, HIGH);
            s_echo_phase = 2;
            s_echo_next  = now + 150;
            break;
        case 2:  // on done -> lamp off, count the blink
            digitalWrite(PIN_AUTO_LAMP, LOW);
            s_echo_blinks_left--;
            s_echo_next  = now + 150;
            s_echo_phase = (s_echo_blinks_left > 0) ? 3 : 4;
            break;
        case 3:  // inter-blink gap done -> next blink on
            digitalWrite(PIN_AUTO_LAMP, HIGH);
            s_echo_phase = 2;
            s_echo_next  = now + 150;
            break;
        default: // digit finished -> restore resting state, go idle
            digitalWrite(PIN_AUTO_LAMP, phone.autoRingEnabled() ? HIGH : LOW);
            s_echo_phase = 0;
            break;
    }
}

static void onDigit(uint8_t digit) {
    Serial.printf("[app] digit: %d\n", digit);
    if (g_dial_confirm) queueDigitEcho(digit);
}

static void onNumber(const char* number) {
    Serial.printf("[app] number: %s\n", number);
    logger.callLog("DIAL number=%s", number);
}

static void onHook(HookState state) {
    Serial.printf("[app] hook: %s\n",
                  state == HookState::OFF_HOOK ? "OFF_HOOK" : "ON_HOOK");
}

static unsigned long dial_tone_start_ms_ = 0;

static void onState(PhoneState state) {
    switch (state) {
    case PhoneState::RINGING:
        logger.callLog("INCOMING ring_start");
        stats.recordIncomingRing();
        break;
    case PhoneState::PLAYING_HISTORY:
        logger.callLog("INCOMING answered");
        stats.recordIncomingAnswered();
        stats.recordPickup();
        stats.callStarted();
        break;
    case PhoneState::DIAL_TONE:
        stats.recordPickup();
        dial_tone_start_ms_ = millis();
        break;
    case PhoneState::DIALING: {
        // First digit arrived — record time since dial tone started.
        unsigned long dialToneMs = millis() - dial_tone_start_ms_;
        stats.recordFirstDigit(dialToneMs);
        break;
    }
    case PhoneState::RINGING_TONE:
    case PhoneState::PLAYING_NUMBER:
        if (state == PhoneState::PLAYING_NUMBER) {
            logger.callLog("OUTGOING connected number=%s", phone.dialledNumber());
            stats.recordOutgoingCall(phone.dialledNumber());
        }
        stats.recordCompletion();
        stats.callStarted();
        break;
    case PhoneState::PLAYING_NOT_REC:
        logger.callLog("OUTGOING not_recognised number=%s", phone.dialledNumber());
        stats.recordNotRecognised(phone.dialledNumber());
        stats.recordDiscovery(phone.dialledNumber());
        stats.recordCompletion();
        stats.callStarted();
        break;
    case PhoneState::AWAIT_COINS:
        stats.recordCompletion();
        break;
    case PhoneState::AWAIT_BTN_B:
        logger.callLog("OUTGOING not_recognised (coinbox) number=%s", phone.dialledNumber());
        stats.recordNotRecognised(phone.dialledNumber());
        stats.recordDiscovery(phone.dialledNumber());
        stats.recordCompletion();
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

// Read the remainder of a serial line after a command letter. Waits up to
// 10s for the first character, then reads until CR/LF (1s idle timeout).
static String readSerialLine() {
    String s;
    unsigned long start = millis();
    // Wait for the first character (feed the watchdog so an operator taking
    // their time — e.g. lifting the handset mid-calibration — can't trip it).
    while (!Serial.available() && millis() - start < 10000) {
        esp_task_wdt_reset();
        delay(1);
    }
    unsigned long t = millis();
    while (millis() - t < 1000) {
        esp_task_wdt_reset();
        if (Serial.available()) {
            char ch = Serial.read();
            if (ch == '\r' || ch == '\n') break;
            s += ch;
            t = millis();
        }
    }
    return s;
}

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
        Serial.printf("[cmd] state=%s  hook=%s  line=%d  mode=%s  sd=%s  coinbox=%s(%s)\n",
                      phone.stateName(),
                      phone.line().hookState() == HookState::OFF_HOOK
                          ? "OFF_HOOK" : "ON_HOOK",
                      phone.line().lastRawReading(),
                      phone.autoRingEnabled() ? "AUTO" : "MANUAL",
                      phone.player().sdReady() ? "OK" : "FAIL",
                      phone.coinBox().isInstalled() ? "ACTIVE" : "OFF",
                      phone.coinBox().overrideMode() == 1 ? "forced-on" :
                      phone.coinBox().overrideMode() == 0 ? "forced-off" : "auto");
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
    case 'B': {
        int cur = phone.coinBox().overrideMode();
        int next = (cur == -1) ? 0 : (cur == 0) ? 1 : -1;
        phone.coinBox().setOverride(next);
        break;
    }
    case 'L': {
        // Get/set master line level (0-100). "L" prints, "L 15" sets.
        String arg = readSerialLine();
        arg.trim();
        if (arg.length()) {
            int v = arg.toInt();
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            phone.player().setLineLevel((uint8_t)v);
        }
        Serial.printf("[cmd] line level = %d%%\n", phone.player().lineLevel());
        break;
    }
    case 'D': {
        g_audio_hook_debug = !g_audio_hook_debug;
        Serial.printf("[cmd] I2S hook debug %s\n", g_audio_hook_debug ? "ON" : "OFF");
        break;
    }
    case 'N': {
        g_line_debug = !g_line_debug;
        Serial.printf("[cmd] line/dial debug %s\n", g_line_debug ? "ON" : "OFF");
        break;
    }
    case 'T': {
        // Bring-up self-test checklist.
        Serial.println(web.selfTest());
        break;
    }
    case 'Q': {
        // Audio probe: play 1 kHz and report peak/RMS of the digital feed.
        Serial.println(web.audioProbe());
        break;
    }
    case 'E': {
        g_dial_confirm = !g_dial_confirm;
        Serial.printf("[cmd] dial echo %s\n", g_dial_confirm ? "ON" : "OFF");
        break;
    }
    case 'I': {
        // Toggle the earpiece dial-pulse clicks.
        phone.setDialTicks(!phone.dialTicks());
        web.persistSettings();
        Serial.printf("[cmd] dial ticks %s\n", phone.dialTicks() ? "ON" : "OFF");
        break;
    }
    case 'K': {
        // Line-sense calibration wizard.
        Serial.println("[cal] Ensure handset is ON-HOOK, then press Enter...");
        readSerialLine();
        int on = phone.line().readAveraged(128);
        Serial.printf("[cal] on-hook raw=%d\n", on);
        Serial.println("[cal] LIFT the handset (OFF-HOOK), then press Enter...");
        readSerialLine();
        int off = phone.line().readAveraged(128);
        Serial.printf("[cal] off-hook raw=%d\n", off);
        if (phone.line().applyCalibration(on, off)) {
            web.persistSettings();
            Serial.printf("[cal] thresholds set: on>=%d  off<%d  (saved)\n",
                          phone.line().thresholdOn(), phone.line().thresholdOff());
        } else {
            Serial.println("[cal] FAILED: on/off levels too close. Check wiring, retry.");
        }
        break;
    }
    case 'F': {
        // Tone/EQ preset test. "F 0".."F 3". Set BEFORE starting playback.
        String arg = readSerialLine();
        arg.trim();
        int preset = arg.length() ? arg.toInt() : 0;
        switch (preset) {
        case 0: phone.player().setEq(0, 0, 0);      break;  // flat
        case 1: phone.player().setEq(-20, 0, 0);    break;  // gentle low cut
        case 2: phone.player().setEq(-40, 0, -12);  break;  // telephone band
        case 3: phone.player().setEq(-40, -6, -40); break;  // narrow mid only
        default: Serial.println("[cmd] F 0=flat 1=lowcut 2=telephone 3=narrow"); break;
        }
        break;
    }
    case 'P': {
        // Play a file directly, regardless of hook state. Reads the rest of
        // the line as the path, e.g.  "P /history/test.wav".
        String path = readSerialLine();
        path.trim();
        if (!path.length()) {
            Serial.println("[cmd] usage: P <path>   e.g. P /history/test.wav");
        } else if (!phone.player().sdReady()) {
            Serial.println("[cmd] error: SD not available");
        } else {
            bool ok = phone.player().playFile(path.c_str(), false);
            Serial.printf("[cmd] %s %s\n", ok ? "playing" : "error playing", path.c_str());
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

    // Start boot indication immediately — fast lamp flash.
    pinMode(PIN_AUTO_LAMP, OUTPUT);
    digitalWrite(PIN_AUTO_LAMP, HIGH);

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

        // Attempt automatic rollback to previous firmware if available.
        const esp_partition_t* prev = esp_ota_get_last_invalid_partition();
        if (prev) {
            Serial.printf("[app] rolling back to previous firmware on %s\n", prev->label);
            esp_ota_set_boot_partition(prev);
            delay(500);
            ESP.restart();
        }

        // Still need SD for the web file manager.
        SPI.begin();
        SD.begin(PIN_SD_CS);
        logger.begin();
        logger.systemLog("SAFE MODE entered after %d crashes (no rollback available)", boot_crash_count);
        stats.begin();
        web.begin(logger, stats, phone);
        return;
    }

    // Blink lamp during init (100ms on/off = fast flash).
    digitalWrite(PIN_AUTO_LAMP, LOW);
    delay(100);
    digitalWrite(PIN_AUTO_LAMP, HIGH);

    phone.onDigit(onDigit);
    phone.onNumber(onNumber);
    phone.onHook(onHook);
    phone.onState(onState);
    phone.begin();
    logger.begin();
    stats.begin();

    Serial.println("[app] commands: R=ring  H=hangup  C=cancel  S=status  A=auto-ring  V0-9=vol  P <path>=play file");
    Serial.println("[app] diagnostics: T=self-test  K=calibrate line  E=dial echo  I=dial ticks  Q=audio probe  N=line debug");
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
    if (!phone.player().sdReady()) {
        stats.recordError(StatsTracker::ErrorType::SD_FAILURE, "SD card not available at boot");
    }

    // Mark current firmware as valid (A/B rollback support).
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("[app] firmware marked valid");

    // Capture the on-hook line-sense level at boot and persist it (tagged with
    // the boot number) so line-level drift is visible across power cycles.
    int boot_line = phone.line().readAveraged(64);
    stats.recordBootLine(logger.bootNumber(), boot_line);
    logger.systemLog("LINE boot-level raw=%d cal=%s on>=%d off<%d",
                     boot_line, phone.line().calibrated() ? "yes" : "no",
                     phone.line().thresholdOn(), phone.line().thresholdOff());

    // Boot complete — single bell strike and steady lamp.
    phone.bell().strike(150);
    digitalWrite(PIN_AUTO_LAMP, phone.autoRingEnabled() ? HIGH : LOW);
    Serial.println("[app] boot complete — ready");

    // Hardware watchdog: reboot if loop() stops for 15 seconds.
    esp_task_wdt_init(15, true);
    esp_task_wdt_add(NULL);
}

// Periodic heartbeat: a liveness line in the system log with uptime and heap
// so an unattended exhibit's health can be reviewed after the fact.
static const unsigned long HEARTBEAT_INTERVAL_MS = 3600000;  // hourly
static unsigned long s_last_heartbeat_ms = 0;
static uint32_t      s_min_heap = 0xFFFFFFFF;

static void serviceHeartbeat() {
    uint32_t heap = ESP.getFreeHeap();
    if (heap < s_min_heap) s_min_heap = heap;

    if (millis() - s_last_heartbeat_ms < HEARTBEAT_INTERVAL_MS) return;
    s_last_heartbeat_ms = millis();
    logger.systemLog("HEARTBEAT uptime=%lus heap=%u min_heap=%u sd=%s state=%s",
                     millis() / 1000, heap, s_min_heap,
                     phone.player().sdReady() ? "ok" : "FAIL",
                     phone.stateName());
}

void loop() {
    esp_task_wdt_reset();

    // Once we've been running for STABLE_BOOT_MS, clear the crash counter.
    if (boot_crash_count > 0 && millis() > STABLE_BOOT_MS) {
        boot_crash_count = 0;
    }

    if (!safe_mode) {
        phone.update();
        bool sd_was_ok = phone.player().sdReady();
        // checkSdCard() auto-remounts a lost card and rebuilds the audio path,
        // so a wedged SD/audio subsystem self-heals without a power cycle.
        phone.player().checkSdCard();
        bool sd_now_ok = phone.player().sdReady();
        if (sd_was_ok && !sd_now_ok) {
            stats.recordError(StatsTracker::ErrorType::SD_FAILURE, "SD card lost during operation");
            logger.systemLog("ERROR: SD card lost — attempting auto-recovery");
        } else if (!sd_was_ok && sd_now_ok) {
            logger.systemLog("RECOVERED: SD card remounted, audio path reinitialised");
        }
    }
    stats.update();
    web.update();
    serviceDialEcho();
    serviceHeartbeat();
    handleSerial();
}
