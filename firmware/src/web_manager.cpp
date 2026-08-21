#include "web_manager.h"
#include "phone_controller.h"
#include "config.h"
#include "portal_html.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

static WebServer server(80);
static Logger* s_logger = nullptr;
static StatsTracker* s_stats = nullptr;
static PhoneController* s_phone = nullptr;

static void saveSettings();

// --- Wi-Fi configuration ----------------------------------------------------
// s_wifi_sta = requested mode from settings (false = host own AP, true = join
// an existing network). s_ap_active reflects the mode actually running after
// boot (a failed STA join falls back to AP, so these can differ).
static bool   s_wifi_sta   = false;
static String s_sta_ssid;
static String s_sta_pass;
static bool   s_ap_active  = true;

// Current portal IP for captive-portal redirects (AP or STA address).
static IPAddress currentIP() { return s_ap_active ? WiFi.softAPIP() : WiFi.localIP(); }

// Minimal JSON string escaping (quotes / backslashes) for user-entered values
// such as Wi-Fi SSIDs.
static String jsonEscape(const String& in) {
    String out;
    for (size_t i = 0; i < in.length(); i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c >= 0x20)        { out += (char)c; }
    }
    return out;
}

// Audio-probe instrumentation (defined in audio_player.cpp).
extern volatile bool g_audio_probe;
void audio_probe_reset();
void audio_probe_result(int32_t* peak, float* rms, uint32_t* count);

// Rotary self-confirm mode (defined in main.cpp): echoes each decoded digit
// on the panel lamp so dialling can be verified with no laptop attached.
extern volatile bool g_dial_confirm;

// Captured on-hook level between the two calibration steps (web flow).
static int s_cal_onhook = -1;

// Run the one-shot bring-up self-test and return a plain-text checklist.
static String buildSelfTest(PhoneController& p) {
    String o = "=== K6 bring-up self-test ===\n";

    o += String("[") + (p.player().sdReady() ? "PASS" : "FAIL") + "] SD card mounted\n";

    int raw = p.line().readAveraged(64);
    bool lineOk = raw < p.line().thresholdOn();  // on-hook should read low
    o += String("[") + (lineOk ? "PASS" : "WARN") + "] line-sense raw=" + raw;
    o += "  (on>=" + String(p.line().thresholdOn()) + " off<" + String(p.line().thresholdOff()) + ")";
    if (!lineOk) o += "  <- high: handset off-hook, line shorted, or needs calibrate";
    o += "\n";

    p.bell().strike(150);
    o += "[ -- ] bell strike fired (listen/feel for a tick; needs 48 V for sound)\n";

    bool tone = p.player().sdReady() && p.player().playTestTone(1000, 2);
    o += String("[") + (tone ? "PASS" : "FAIL") + "] 1 kHz test tone -> earpiece (2 s)\n";

    // Buttons are active-low; report their instantaneous state to catch a
    // stuck/shorted button (reads DOWN when nothing is pressed).
    o += "buttons now: ";
    o += "RING=" + String(digitalRead(PIN_BTN_RING)   ? "up" : "DOWN") + "  ";
    o += "CANCEL=" + String(digitalRead(PIN_BTN_CANCEL) ? "up" : "DOWN") + "  ";
    o += "RESET=" + String(digitalRead(PIN_BTN_RESET)  ? "up" : "DOWN") + "  ";
    o += "MODE=" + String(digitalRead(PIN_BTN_MODE)    ? "up" : "DOWN") + "\n";

    o += "coinbox: " + String(p.coinBox().isInstalled() ? "detected" : "none") + "\n";
    o += "heap free: " + String(ESP.getFreeHeap()) + " bytes\n";
    o += "=== end ===";

    // Persist the result so it survives a reboot and drift can be reviewed.
    if (s_stats) s_stats->recordSelfTest(o.c_str(), millis() / 1000);
    return o;
}

// Play a fixed 1 kHz tone and measure the peak/RMS of the samples fed to I2S.
static String buildProbe(PhoneController& p) {
    if (!p.player().sdReady()) return "PROBE: SD not available";

    audio_probe_reset();
    g_audio_probe = true;
    if (!p.player().playTestTone(1000, 3)) {
        g_audio_probe = false;
        return "PROBE: could not start test tone";
    }

    // Feed I2S for ~2 s while the probe accumulates.
    unsigned long start = millis();
    while (millis() - start < 2000) {
        p.player().update();
        esp_task_wdt_reset();
        delay(2);
    }
    g_audio_probe = false;
    p.player().stop();

    int32_t peak = 0; float rms = 0; uint32_t cnt = 0;
    audio_probe_result(&peak, &rms, &cnt);

    float peakPct = peak * 100.0f / 32767.0f;
    float rmsPct  = rms  * 100.0f / 32767.0f;
    // For a clean sine, peak/RMS = sqrt(2) ~ 1.414. Big deviations hint at
    // clipping (ratio -> 1.0) or a mostly-silent/distorted feed.
    float crest = (rms > 1.0f) ? (peak / rms) : 0.0f;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "=== audio probe (1 kHz, digital side) ===\n"
             "samples=%lu\n"
             "peak=%ld/32767 (%.1f%%)\n"
             "rms=%.0f/32767 (%.2f%%)\n"
             "crest(peak/rms)=%.2f (sine~1.41)\n"
             "note: measures the DIGITAL feed. If this is clean but the\n"
             "earpiece is distorted, the fault is analog (amp/transformer).",
             (unsigned long)cnt, (long)peak, peakPct, rms, rmsPct, crest);
    return String(buf);
}

// --- HTML UI (served from flash, not SD) ------------------------------------

// The portal UI is a compiled single-file React app, gzip-compressed and
// embedded as a byte array (see portal_html.h). Served with Content-Encoding:
// gzip from handleIndex(). Source project lives under tools/portal.

// --- API handlers -----------------------------------------------------------

// The compressed portal is ~120 kB, which over a weak AP link can take longer
// than the task watchdog window to push out. A single blocking send therefore
// risks a watchdog reboot mid-page-load (the page never arrives, and the reboot
// drops the client). Stream it in small chunks and feed the watchdog between
// each one, and give up early if the browser goes away.
static void handleIndex() {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.setContentLength(PORTAL_HTML_GZ_LEN);
    server.send(200, "text/html", "");

    WiFiClient client = server.client();
    constexpr size_t CHUNK = 1024;
    for (size_t sent = 0; sent < PORTAL_HTML_GZ_LEN; ) {
        if (!client.connected()) break;
        size_t n = PORTAL_HTML_GZ_LEN - sent;
        if (n > CHUNK) n = CHUNK;
        size_t wrote = client.write(PORTAL_HTML_GZ + sent, n);
        esp_task_wdt_reset();
        if (wrote == 0) break;
        sent += wrote;
    }
}

// Minimal, dependency-free recovery page. The main portal is a large compiled
// app; if it ever fails to load, this stays reachable at /recovery so firmware
// can still be re-flashed and the board rebooted without a USB cable.
static const char RECOVERY_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>K6 Recovery</title>
<style>body{font-family:sans-serif;margin:2em;max-width:34em}
h1{font-size:1.3em}button{padding:.6em 1.2em;margin-top:.5em}
fieldset{margin-bottom:1.5em}</style></head><body>
<h1>K6 Exhibit &mdash; Recovery</h1>
<p>Minimal page for when the main portal will not load.</p>
<fieldset><legend>Firmware update</legend>
<form method="POST" action="/api/ota" enctype="multipart/form-data">
<input type="file" name="file" accept=".bin" required>
<button type="submit">Upload &amp; install</button></form>
<p><small>Takes a minute; the board reboots when finished.</small></p>
</fieldset>
<fieldset><legend>Other</legend>
<button onclick="fetch('/api/reboot',{method:'POST'})">Reboot</button>
<button onclick="location.href='/api/status'">View status JSON</button>
<button onclick="location.href='/'">Try main portal</button>
</fieldset></body></html>
)rawhtml";

static void handleRecovery() {
    server.send_P(200, "text/html", RECOVERY_HTML);
}

static void handleFileList() {
    String path = server.arg("path");
    if (path.isEmpty()) path = "/";
    if (!path.endsWith("/")) path += "/";

    // Some FS layers refuse a directory path with a trailing slash, so open the
    // bare form ("/numbers" rather than "/numbers/"); root stays as "/".
    String openPath = path;
    while (openPath.length() > 1 && openPath.endsWith("/")) {
        openPath.remove(openPath.length() - 1);
    }

    File dir = SD.open(openPath);
    if (!dir || !dir.isDirectory()) {
        server.send(200, "application/json", "[]");
        return;
    }

    String json = "[";
    bool first = true;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (!first) json += ",";
        first = false;
        // name() is the bare entry name on this core, but has returned a full
        // path on others -- keep only the final segment either way.
        String name = entry.name();
        int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);

        json += "{\"name\":\"";
        json += jsonEscape(name);
        json += "\",\"size\":";
        json += String(entry.size());
        json += ",\"dir\":";
        json += entry.isDirectory() ? "true" : "false";
        json += "}";
        entry.close();
    }
    dir.close();
    json += "]";
    server.send(200, "application/json", json);
}

// MP3 sync word check: valid MP3 frames start with 0xFF 0xFB/FA/F3/F2
// (11 sync bits set).  We also accept ID3 tags (start with "ID3").
static bool looksLikeMp3(const uint8_t* buf, size_t len) {
    if (len < 3) return false;
    // ID3v2 tag header
    if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') return true;
    // MPEG sync word: first byte 0xFF, second byte has upper 3 bits set (0xE0)
    if (buf[0] == 0xFF && (buf[1] & 0xE0) == 0xE0) return true;
    return false;
}

static bool s_upload_valid = true;
static String s_upload_path;

static void handleUpload() {
    HTTPUpload& upload = server.upload();
    static File uploadFile;

    // A large file blocks server.handleClient() for the whole transfer, so
    // loop()'s watchdog reset never runs. Feed it here (this callback fires
    // per chunk) or a slow upload trips the 15s watchdog and reboots.
    esp_task_wdt_reset();

    if (upload.status == UPLOAD_FILE_START) {
        String path = server.arg("path");
        if (!path.endsWith("/")) path += "/";
        path += upload.filename;
        s_upload_path = path;
        s_upload_valid = true;
        Serial.printf("[web] upload: %s\n", path.c_str());
        uploadFile = SD.open(path, FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            // Validate first chunk of .mp3 files.  WAV files are accepted
            // as-is (the player supports MP3 and WAV).
            if (s_upload_valid && upload.totalSize == 0 &&
                (s_upload_path.endsWith(".mp3") || s_upload_path.endsWith(".MP3"))) {
                if (!looksLikeMp3(upload.buf, upload.currentSize)) {
                    s_upload_valid = false;
                    Serial.printf("[web] REJECTED: not a valid MP3: %s\n", s_upload_path.c_str());
                }
            }
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            if (!s_upload_valid) {
                // Remove invalid file.
                SD.remove(s_upload_path);
                Serial.printf("[web] removed invalid MP3: %s\n", s_upload_path.c_str());
            } else {
                Serial.printf("[web] upload complete: %u bytes\n", upload.totalSize);
            }
        }
    }
}

static void handleUploadComplete() {
    if (!s_upload_valid) {
        server.send(200, "application/json",
                    "{\"ok\":false,\"error\":\"Invalid MP3 file — not a valid audio file\"}");
    } else {
        server.send(200, "application/json", "{\"ok\":true}");
    }
}

static void handleDelete() {
    String path = server.arg("path");
    if (path.isEmpty() || path == "/") {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Invalid path\"}");
        return;
    }
    if (SD.exists(path)) {
        SD.remove(path);
        server.send(200, "application/json", "{\"ok\":true}");
    } else {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"File not found\"}");
    }
}

static void handleMkdir() {
    String path = server.arg("path");
    if (path.isEmpty()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Invalid path\"}");
        return;
    }
    SD.mkdir(path);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleCoinMode() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < -1) v = -1;
    if (v > 1)  v = 1;
    s_phone->coinBox().setOverride(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Coin box override set to %d via web", v);
    String json = "{\"ok\":true,\"active\":";
    json += s_phone->coinBox().isInstalled() ? "true" : "false";
    json += "}";
    server.send(200, "application/json", json);
}

static void handleStatus() {
    if (!s_phone) {
        server.send(200, "application/json", "{\"error\":\"not ready\"}");
        return;
    }

    String json = "{";
    json += "\"heap\":";     json += String(ESP.getFreeHeap());
    json += ",\"sd\":";      json += SD.cardType() != CARD_NONE ? "true" : "false";
    json += ",\"sd_total\":"; json += String((uint32_t)(SD.totalBytes() / (1024 * 1024)));
    json += ",\"sd_used\":";  json += String((uint32_t)(SD.usedBytes() / (1024 * 1024)));
    json += ",\"uptime\":";  json += String(millis() / 1000);
    json += ",\"firmware\":\"" FIRMWARE_VERSION "\"";
    json += ",\"volume\":";  json += String(s_phone->player().getVolume());
    json += ",\"bell_freq\":"; json += String(s_phone->bell().ringFreq());
    json += ",\"digit_gap\":"; json += String(s_phone->numberCompleteMs());
    json += ",\"line_level\":"; json += String(s_phone->player().lineLevel());
    json += ",\"line_cal\":"; json += s_phone->line().calibrated() ? "true" : "false";
    json += ",\"errors\":"; json += String(s_stats ? s_stats->errorCount() : 0);
    json += ",\"ar_min\":";  json += String(s_phone->autoRingMinMs());
    json += ",\"ar_max\":";  json += String(s_phone->autoRingMaxMs());
    json += ",\"ring_max\":"; json += String(s_phone->maxRingCadences());
    json += ",\"rt_min\":"; json += String(s_phone->ringToneMinSecs());
    json += ",\"rt_max\":"; json += String(s_phone->ringToneMaxSecs());
    json += ",\"alert_idle\":"; json += String(s_phone->alertIdleMinutes());
    json += ",\"alert_on\":"; json += s_phone->isAlertActive() ? "true" : "false";
    json += ",\"coin_override\":"; json += String(s_phone->coinBox().overrideMode());
    json += ",\"coin_active\":"; json += s_phone->coinBox().isInstalled() ? "true" : "false";
    json += ",\"wifi_mode\":\""; json += s_ap_active ? "ap" : "sta"; json += "\"";
    json += ",\"wifi_ssid\":\""; json += jsonEscape(s_ap_active ? WiFi.softAPSSID() : WiFi.SSID()); json += "\"";
    json += ",\"wifi_ip\":\"";   json += currentIP().toString(); json += "\"";
    json += ",\"wifi_cfg_mode\":\""; json += s_wifi_sta ? "sta" : "ap"; json += "\"";
    json += ",\"wifi_cfg_ssid\":\""; json += jsonEscape(s_sta_ssid); json += "\"";
    json += ",\"mode\":\"";  json += s_phone->autoRingEnabled() ? "AUTO" : "MANUAL";
    json += "\",\"state\":\""; json += s_phone->stateName();
    json += "\",\"playing\":\"";
    if (s_phone->player().isPlaying()) {
        json += s_phone->player().currentFile();
    }
    json += "\",\"call_secs\":";
    if (s_phone->state() != PhoneState::IDLE) {
        json += String((millis() - s_phone->stateEnterTime()) / 1000);
    } else {
        json += "-1";
    }
    json += "}";
    server.send(200, "application/json", json);
}

static void handleRollback() {
    const esp_partition_t* prev = esp_ota_get_last_invalid_partition();
    if (!prev) {
        // Try the non-running OTA partition as fallback.
        const esp_partition_t* running = esp_ota_get_running_partition();
        const esp_partition_t* other = esp_ota_get_next_update_partition(running);
        if (other && other != running) prev = other;
    }
    if (!prev) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No previous firmware available\"}");
        return;
    }
    esp_err_t err = esp_ota_set_boot_partition(prev);
    if (err != ESP_OK) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Rollback failed\"}");
        return;
    }
    if (s_logger) s_logger->systemLog("Firmware rollback to %s via web", prev->label);
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
}

static void handleOTA() {
    server.send(200, "application/json",
                Update.hasError()
                    ? "{\"ok\":false,\"error\":\"Update failed\"}"
                    : "{\"ok\":true}");
    if (!Update.hasError()) {
        delay(500);
        ESP.restart();
    }
}

static void handleOTAUpload() {
    HTTPUpload& upload = server.upload();

    esp_task_wdt_reset();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[web] OTA start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[web] OTA complete: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

// --- Log API handlers -------------------------------------------------------

// Stream a heap text buffer to the client without forcing a single large
// String allocation. Passing a long char* to server.send() makes the core
// build a String copy that fails ("String cast failed") under heap pressure
// and leaves a slow/closed client mid-write (the fd EAGAIN spam). Writing the
// response directly avoids that, and avoids the core's "content length is
// zero" warning that an empty send() would emit before sendContent().
static void sendTextBuffer(const char* buf, size_t len) {
    WiFiClient client = server.client();
    client.print(F("HTTP/1.1 200 OK\r\n"));
    client.print(F("Content-Type: text/plain\r\n"));
    client.printf("Content-Length: %u\r\n", (unsigned)len);
    client.print(F("Connection: close\r\n\r\n"));
    client.write((const uint8_t*)buf, len);
}

static void handleLogSystem() {
    if (!s_logger) { server.send(200, "text/plain", "(empty)"); return; }
    size_t len;
    char* buf = s_logger->readSystemLog(&len);
    if (buf) {
        sendTextBuffer(buf, len);
        free(buf);
    } else {
        server.send(200, "text/plain", "(empty)");
    }
}

static void handleLogCalls() {
    if (!s_logger) { server.send(200, "text/plain", "(empty)"); return; }
    size_t len;
    char* buf = s_logger->readCallLog(&len);
    if (buf) {
        sendTextBuffer(buf, len);
        free(buf);
    } else {
        server.send(200, "text/plain", "(empty)");
    }
}

static void handleLogClear() {
    if (!s_logger) { server.send(200, "application/json", "{\"ok\":true}"); return; }
    String which = server.arg("log");
    if (which == "system" || which == "all") s_logger->clearSystemLog();
    if (which == "calls"  || which == "all") s_logger->clearCallLog();
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Control API handlers ---------------------------------------------------

static void handleVolume() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < 0) v = 0;
    if (v > 21) v = 21;
    s_phone->player().setVolume(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Volume set to %d via web", v);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDigitGap() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int secs = server.arg("v").toInt();
    if (secs < 2)  secs = 2;
    if (secs > 30) secs = 30;
    s_phone->setNumberCompleteMs((unsigned long)secs * 1000UL);
    saveSettings();
    if (s_logger) s_logger->systemLog("Inter-digit gap set to %ds via web", secs);
    server.send(200, "application/json", "{\"ok\":true}");
}

// Configure Wi-Fi mode (host own AP vs join existing network). Persists the
// choice and reboots so the new mode takes effect from a clean boot.
static void handleWifi() {
    String mode = server.arg("mode");
    if (mode == "sta") {
        s_wifi_sta  = true;
        s_sta_ssid  = server.arg("ssid");
        s_sta_pass  = server.arg("pass");
    } else {
        s_wifi_sta = false;
    }
    saveSettings();
    if (s_logger) s_logger->systemLog("Wi-Fi mode set to %s via web (ssid=%s), rebooting",
                                       s_wifi_sta ? "STA" : "AP", s_sta_ssid.c_str());
    server.send(200, "application/json", "{\"ok\":true}");
    delay(400);
    ESP.restart();
}

static void handleLineLevel() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    s_phone->player().setLineLevel(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Line level set to %d%% via web", v);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleBellFreq() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int hz = server.arg("hz").toInt();
    if (hz < 10) hz = 10;
    if (hz > 50) hz = 50;
    s_phone->bell().setRingFreq(hz);
    saveSettings();
    if (s_logger) s_logger->systemLog("Bell frequency set to %d Hz via web", hz);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleAutoRing() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    unsigned long minMin = server.arg("min").toInt();
    unsigned long maxMin = server.arg("max").toInt();
    if (minMin < 1) minMin = 1;
    if (maxMin < minMin) maxMin = minMin;
    if (maxMin > 120) maxMin = 120;
    s_phone->setAutoRingInterval(minMin * 60000, maxMin * 60000);
    saveSettings();
    if (s_logger) s_logger->systemLog("Auto-ring set to %lu-%lu min via web", minMin, maxMin);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRingNow() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    s_phone->ring();
    if (s_logger) s_logger->systemLog("Ring triggered via web");
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleTone() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int hz = server.arg("hz").toInt();
    int secs = server.arg("secs").toInt();
    if (hz < 50) hz = 1000;
    if (secs < 1) secs = 5;
    bool ok = s_phone->player().playTestTone(hz, secs);
    if (s_logger) s_logger->systemLog("Test tone %d Hz for %ds via web", hz, secs);
    server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void handleTestRing() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int secs = server.arg("secs").toInt();
    if (secs < 1) secs = 3;
    if (secs > 30) secs = 30;
    s_phone->testRing(secs);
    if (s_logger) s_logger->systemLog("Test ring (%ds) triggered via web", secs);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleAlertIdle() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < 0) v = 0;
    if (v > 1440) v = 1440;
    s_phone->setAlertIdleMinutes(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Alert idle set to %d min via web", v);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRingTone() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int mn = server.arg("min").toInt();
    int mx = server.arg("max").toInt();
    s_phone->setRingToneRange(mn, mx);
    saveSettings();
    if (s_logger) s_logger->systemLog("Ring tone set to %d-%ds via web", s_phone->ringToneMinSecs(), s_phone->ringToneMaxSecs());
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRingCount() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int n = server.arg("n").toInt();
    if (n < 0) n = 0;
    if (n > 60) n = 60;
    s_phone->setMaxRingCadences(n);
    saveSettings();
    if (s_logger) s_logger->systemLog("Ring count set to %d via web", n);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleToggleMode() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    s_phone->toggleAutoRing();
    if (s_logger) s_logger->systemLog("Mode toggled to %s via web",
                                       s_phone->autoRingEnabled() ? "AUTO" : "MANUAL");
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Terminal command handler -----------------------------------------------
// Text-based command interface for manual control / troubleshooting from the
// web dashboard.  Returns plain text.
static void handleTerminal() {
    if (!s_phone) { server.send(200, "text/plain", "error: phone not ready"); return; }
    String cmd = server.arg("cmd");
    cmd.trim();
    if (cmd.length() == 0) { server.send(200, "text/plain", "(no command)"); return; }

    String verb = cmd, arg = "";
    int sp = cmd.indexOf(' ');
    if (sp >= 0) { verb = cmd.substring(0, sp); arg = cmd.substring(sp + 1); arg.trim(); }
    verb.toLowerCase();
    String larg = arg; larg.toLowerCase();

    PhoneController& p = *s_phone;
    String out;

    if (verb == "help" || verb == "?") {
        out  = "Available commands:\n";
        out += "  status              show phone state\n";
        out += "  ring                trigger the bell\n";
        out += "  testring [secs]     ring for a fixed time (default 3s)\n";
        out += "  hangup              hang up / stop playback\n";
        out += "  cancel              cancel ringing\n";
        out += "  mode [auto|manual]  get/set ring mode\n";
        out += "  vol [0-21]          get/set handset volume\n";
        out += "  bellfreq [10-50]    get/set ring frequency (Hz)\n";
        out += "  digitgap [2-30]     get/set seconds allowed between dialled digits\n";
        out += "  linelevel [0-100]   get/set master line level (%)\n";
        out += "  coin [auto|on|off]  coin box override\n";
        out += "  play <path>         play an SD file\n";
        out += "  tone [hz] [secs]    play a steady sine tone (default 1000Hz 5s)\n";
        out += "  stop                stop playback\n";
        out += "  ls [path]           list SD directory\n";
        out += "  cat <path>          show a text file\n";
        out += "  rm <path>           delete a file\n";
        out += "  sd                  SD card info\n";
        out += "  mem                 free heap\n";
        out += "  uptime              time since boot\n";
        out += "  wifi                Wi-Fi connection info\n";
        out += "  selftest            run bring-up self-test checklist\n";
        out += "  probe               play 1kHz tone, report peak/RMS\n";
        out += "  calibrate on        capture ON-HOOK line level (handset down)\n";
        out += "  calibrate off       capture OFF-HOOK + set thresholds (handset up)\n";
        out += "  dialecho [on|off]   echo dialled digits on the panel lamp\n";
        out += "  ticks [on|off]      earpiece click on each rotary dial pulse\n";
        out += "  reboot              restart the device";
    } else if (verb == "status" || verb == "s") {
        int m = p.coinBox().overrideMode();
        out  = "state=" + String(p.stateName());
        out += "  hook=" + String(p.line().hookState() == HookState::OFF_HOOK ? "OFF_HOOK" : "ON_HOOK");
        out += "  line=" + String(p.line().lastRawReading());
        out += "  mode=" + String(p.autoRingEnabled() ? "AUTO" : "MANUAL");
        out += "  sd=" + String(p.player().sdReady() ? "OK" : "FAIL");
        out += "  vol=" + String(p.player().getVolume()) + "/21";
        out += "  coinbox=" + String(p.coinBox().isInstalled() ? "ACTIVE" : "OFF");
        out += "(" + String(m == 1 ? "forced-on" : m == 0 ? "forced-off" : "auto") + ")";
    } else if (verb == "ring") {
        p.ring(); out = "ringing";
    } else if (verb == "testring") {
        int secs = arg.length() ? arg.toInt() : 3;
        if (secs < 1) secs = 1; if (secs > 30) secs = 30;
        p.testRing(secs);
        out = "test ring for " + String(secs) + "s";
    } else if (verb == "hangup" || verb == "h") {
        p.hangUp(); out = "hung up";
    } else if (verb == "cancel" || verb == "c") {
        p.cancelRing(); out = "ring cancelled";
    } else if (verb == "mode") {
        if (larg == "auto")        p.setAutoRing(true);
        else if (larg == "manual") p.setAutoRing(false);
        else if (larg.length())    { server.send(200, "text/plain", "usage: mode [auto|manual]"); return; }
        else { server.send(200, "text/plain", String("mode=") + (p.autoRingEnabled() ? "AUTO" : "MANUAL")); return; }
        saveSettings();
        out = String("mode=") + (p.autoRingEnabled() ? "AUTO" : "MANUAL");
    } else if (verb == "vol") {
        if (arg.length()) {
            int v = arg.toInt(); if (v < 0) v = 0; if (v > 21) v = 21;
            p.player().setVolume(v); saveSettings();
        }
        out = "volume=" + String(p.player().getVolume()) + "/21";
    } else if (verb == "digitgap") {
        if (arg.length()) {
            int secs = arg.toInt(); if (secs < 2) secs = 2; if (secs > 30) secs = 30;
            p.setNumberCompleteMs((unsigned long)secs * 1000UL); saveSettings();
        }
        out = "inter-digit gap=" + String(p.numberCompleteMs() / 1000) + "s";
    } else if (verb == "bellfreq") {
        if (arg.length()) {
            int hz = arg.toInt(); if (hz < 10) hz = 10; if (hz > 50) hz = 50;
            p.bell().setRingFreq(hz); saveSettings();
        }
        out = "bell frequency=" + String(p.bell().ringFreq()) + " Hz";
    } else if (verb == "linelevel") {
        if (arg.length()) {
            int v = arg.toInt(); if (v < 0) v = 0; if (v > 100) v = 100;
            p.player().setLineLevel(v); saveSettings();
        }
        out = "line level=" + String(p.player().lineLevel()) + "%";
    } else if (verb == "coin") {
        if (larg == "auto")      p.coinBox().setOverride(-1);
        else if (larg == "on")   p.coinBox().setOverride(1);
        else if (larg == "off")  p.coinBox().setOverride(0);
        else if (larg.length())  { server.send(200, "text/plain", "usage: coin [auto|on|off]"); return; }
        if (larg.length()) saveSettings();
        int m = p.coinBox().overrideMode();
        out  = String("coin override=") + (m == 1 ? "on" : m == 0 ? "off" : "auto");
        out += "  active=" + String(p.coinBox().isInstalled() ? "yes" : "no");
    } else if (verb == "play") {
        if (!arg.length())           { server.send(200, "text/plain", "usage: play <path>"); return; }
        if (!p.player().sdReady())   { server.send(200, "text/plain", "error: SD not available"); return; }
        bool ok = p.player().playFile(arg.c_str(), false);
        out = ok ? ("playing " + arg) : ("error: could not play " + arg);
    } else if (verb == "tone") {
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        // tone [hz] [secs] — default 1000 Hz for 5s
        int hz = 1000, secs = 5;
        if (larg.length()) {
            int sp2 = larg.indexOf(' ');
            if (sp2 >= 0) { hz = larg.substring(0, sp2).toInt(); secs = larg.substring(sp2 + 1).toInt(); }
            else hz = larg.toInt();
        }
        if (secs < 1) secs = 5;
        bool ok = p.player().playTestTone(hz, secs);
        out = ok ? ("playing " + String(hz) + " Hz tone for " + String(secs) + "s")
                 : "error: could not start tone";
    } else if (verb == "stop") {
        p.player().stop(); out = "playback stopped";
    } else if (verb == "ls") {
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        String path = arg.length() ? arg : "/";
        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) { server.send(200, "text/plain", "error: not a directory: " + path); return; }
        out = path + ":\n";
        File f = dir.openNextFile();
        int n = 0;
        while (f) {
            out += f.isDirectory() ? "  [DIR] " : "        ";
            out += String(f.name());
            if (!f.isDirectory()) out += "  (" + String((unsigned long)f.size()) + " bytes)";
            out += "\n";
            f = dir.openNextFile();
            if (++n > 100) { out += "  ... (more)\n"; break; }
        }
        if (n == 0) out += "  (empty)";
    } else if (verb == "cat") {
        if (!arg.length())         { server.send(200, "text/plain", "usage: cat <path>"); return; }
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        File f = SD.open(arg);
        if (!f || f.isDirectory()) { server.send(200, "text/plain", "error: cannot open " + arg); return; }
        const size_t MAXB = 2048;
        while (f.available() && out.length() < MAXB) out += (char)f.read();
        if (f.available()) out += "\n... (truncated)";
        f.close();
        if (out.length() == 0) out = "(empty file)";
    } else if (verb == "rm") {
        if (!arg.length())         { server.send(200, "text/plain", "usage: rm <path>"); return; }
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        bool ok = SD.remove(arg);
        if (ok && s_logger) s_logger->systemLog("File deleted via terminal: %s", arg.c_str());
        out = ok ? ("deleted " + arg) : ("error: could not delete " + arg);
    } else if (verb == "sd") {
        if (!p.player().sdReady()) out = "SD card: NOT MOUNTED";
        else {
            out  = "SD card: OK  type=" + String(SD.cardType());
            out += "  size=" + String((unsigned long)(SD.cardSize() / (1024 * 1024))) + "MB";
            out += "  used=" + String((unsigned long)(SD.usedBytes() / (1024 * 1024))) + "MB";
        }
    } else if (verb == "mem" || verb == "heap") {
        out  = "free heap: " + String(ESP.getFreeHeap()) + " bytes";
        out += "  min free: " + String(ESP.getMinFreeHeap()) + " bytes";
    } else if (verb == "uptime") {
        unsigned long s = millis() / 1000;
        out = "uptime: " + String(s / 3600) + "h " + String((s % 3600) / 60) + "m " + String(s % 60) + "s";
    } else if (verb == "wifi") {
        if (s_ap_active) {
            out  = "mode: hosting AP\n";
            out += "AP SSID: " + WiFi.softAPSSID() + "\n";
            out += "AP IP: " + WiFi.softAPIP().toString() + "\n";
            out += "connected clients: " + String(WiFi.softAPgetStationNum());
        } else {
            out  = "mode: joined network\n";
            out += "SSID: " + WiFi.SSID() + "\n";
            out += "IP: " + WiFi.localIP().toString() + "\n";
            out += "RSSI: " + String(WiFi.RSSI()) + " dBm";
        }
    } else if (verb == "selftest" || verb == "test") {
        out = buildSelfTest(p);
    } else if (verb == "probe") {
        out = buildProbe(p);
    } else if (verb == "calibrate" || verb == "cal") {
        if (larg == "on" || larg.length() == 0) {
            s_cal_onhook = p.line().readAveraged(128);
            out  = "on-hook level=" + String(s_cal_onhook) + "\n";
            out += "now LIFT the handset and run: calibrate off";
        } else if (larg == "off") {
            if (s_cal_onhook < 0) {
                out = "error: run 'calibrate on' first (handset down)";
            } else {
                int off = p.line().readAveraged(128);
                if (p.line().applyCalibration(s_cal_onhook, off)) {
                    saveSettings();
                    out  = "calibrated: on-hook=" + String(s_cal_onhook);
                    out += " off-hook=" + String(off) + "\n";
                    out += "thresholds -> on>=" + String(p.line().thresholdOn());
                    out += " off<" + String(p.line().thresholdOff()) + " (saved)";
                } else {
                    out  = "FAILED: on-hook=" + String(s_cal_onhook) + " off-hook=" + String(off);
                    out += " — too close (need off-hook much higher). Check wiring.";
                }
                s_cal_onhook = -1;
            }
        } else {
            out = "usage: calibrate on | calibrate off";
        }
    } else if (verb == "dialecho") {
        if (larg == "on")       g_dial_confirm = true;
        else if (larg == "off") g_dial_confirm = false;
        else if (larg.length()) { server.send(200, "text/plain", "usage: dialecho [on|off]"); return; }
        else g_dial_confirm = !g_dial_confirm;
        out = String("dial echo ") + (g_dial_confirm ? "ON" : "OFF");
    } else if (verb == "ticks") {
        if (larg == "on")       s_phone->setDialTicks(true);
        else if (larg == "off") s_phone->setDialTicks(false);
        else if (larg.length()) { server.send(200, "text/plain", "usage: ticks [on|off]"); return; }
        else s_phone->setDialTicks(!s_phone->dialTicks());
        saveSettings();
        out = String("dial ticks ") + (s_phone->dialTicks() ? "ON" : "OFF");
    } else if (verb == "reboot") {
        if (s_logger) s_logger->systemLog("Reboot via terminal");
        server.send(200, "text/plain", "rebooting…");
        delay(300);
        ESP.restart();
        return;
    } else {
        out = "unknown command: " + verb + "  (type 'help')";
    }

    server.send(200, "text/plain", out);
}

// --- Stats API handlers -----------------------------------------------------

static void handleStats() {
    if (!s_stats) { server.send(200, "application/json", "{}"); return; }

    const CallStats& st = s_stats->stats();
    unsigned long session_secs = millis() / 1000;

    String json = "{";
    json += "\"incoming\":";       json += String(st.total_incoming);
    json += ",\"outgoing\":";      json += String(st.total_outgoing);
    json += ",\"answered\":";      json += String(st.total_answered);
    json += ",\"not_recognised\":"; json += String(st.total_not_recognised);
    json += ",\"coin_collected\":"; json += String(st.total_coin_collected);
    json += ",\"coin_refunded\":";  json += String(st.total_coin_refunded);
    json += ",\"total_uptime\":";   json += String(st.uptime_seconds + session_secs);
    json += ",\"call_seconds\":";  json += String(st.total_call_seconds);
    json += ",\"longest_call\":";  json += String(st.longest_call_seconds);
    json += ",\"avg_call\":";      json += String(s_stats->avgCallSeconds());
    json += ",\"call_count\":";    json += String(st.call_count);
    json += ",\"pickups\":";       json += String(st.total_pickups);
    json += ",\"completions\":";   json += String(st.total_completions);
    json += ",\"first_digit_ms\":"; json += String(st.total_first_digit_ms);
    json += ",\"first_digit_n\":";  json += String(st.first_digit_count);

    StatsTracker::NumberEntry top[5];
    int n = s_stats->topNumbers(top, 5);
    json += ",\"top_numbers\":[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"number\":\"";
        json += top[i].number;
        json += "\",\"count\":";
        json += String(top[i].count);
        json += "}";
    }
    json += "]";

    // Since-boot ("today") summary — the exhibit is powered down out of hours.
    const SessionStats& se = s_stats->session();
    json += ",\"session\":{";
    json += "\"incoming\":";       json += String(se.incoming);
    json += ",\"outgoing\":";      json += String(se.outgoing);
    json += ",\"answered\":";      json += String(se.answered);
    json += ",\"not_recognised\":"; json += String(se.not_recognised);
    json += ",\"pickups\":";       json += String(se.pickups);
    json += ",\"completions\":";   json += String(se.completions);
    json += ",\"uptime\":";        json += String(session_secs);
    StatsTracker::NumberEntry stop[5];
    int sn = s_stats->topSessionNumbers(stop, 5);
    json += ",\"top_numbers\":[";
    for (int i = 0; i < sn; i++) {
        if (i > 0) json += ",";
        json += "{\"number\":\"";
        json += stop[i].number;
        json += "\",\"count\":";
        json += String(stop[i].count);
        json += "}";
    }
    json += "]}";
    json += "}";

    server.send(200, "application/json", json);
}

static void handleStatsReset() {
    if (s_stats) {
        s_stats->resetStats();
        if (s_logger) s_logger->systemLog("Stats reset via web");
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Diagnostics API handler ------------------------------------------------

static void handleDiagnostics() {
    if (!s_stats) { server.send(200, "application/json", "[]"); return; }

    int count = s_stats->errorCount();
    const StatsTracker::ErrorEntry* entries = s_stats->errorEntries();

    String json = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        json += "{\"time\":";
        json += String(entries[i].timestamp);
        json += ",\"type\":\"";
        switch (entries[i].type) {
            case StatsTracker::ErrorType::BELL_FAULT:  json += "BELL_FAULT"; break;
            case StatsTracker::ErrorType::LINE_ANOMALY: json += "LINE_ANOMALY"; break;
            case StatsTracker::ErrorType::SD_FAILURE:   json += "SD_FAILURE"; break;
        }
        json += "\"";
        if (entries[i].detail[0]) {
            json += ",\"detail\":\"";
            // Escape any quotes in detail string.
            for (const char* p = entries[i].detail; *p; p++) {
                if (*p == '"') json += "\\\"";
                else if (*p == '\\') json += "\\\\";
                else json += *p;
            }
            json += "\"";
        }
        json += "}";
    }
    json += "]";

    server.send(200, "application/json", json);
}

// Persisted diagnostics: boot-time line-sense readings (drift) + last self-test.
static void handleDiag() {
    if (!s_stats) { server.send(200, "application/json", "{}"); return; }

    String json = "{\"boot_lines\":[";
    int n = s_stats->bootLineCount();
    const StatsTracker::BootLineEntry* bl = s_stats->bootLineEntries();
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"boot\":"; json += String(bl[i].boot);
        json += ",\"raw\":";  json += String(bl[i].raw);
        json += "}";
    }
    json += "],\"selftest_uptime\":";
    json += String(s_stats->lastSelfTestUptime());
    json += ",\"selftest\":\"";
    for (const char* p = s_stats->lastSelfTest(); *p; p++) {
        if (*p == '"') json += "\\\"";
        else if (*p == '\\') json += "\\\\";
        else if (*p == '\n') json += "\\n";
        else if (*p == '\r') { /* skip */ }
        else json += *p;
    }
    json += "\"}";
    server.send(200, "application/json", json);
}

// --- Discovery log API handlers ---------------------------------------------

static void handleDiscovery() {
    if (!s_stats) { server.send(200, "application/json", "[]"); return; }

    int count = s_stats->discoveryCount();
    const StatsTracker::NumberEntry* entries = s_stats->discoveryEntries();

    String json = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        json += "{\"number\":\"";
        json += entries[i].number;
        json += "\",\"count\":";
        json += String(entries[i].count);
        json += "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

static void handleDiscoveryClear() {
    if (s_stats) {
        s_stats->clearDiscovery();
        if (s_logger) s_logger->systemLog("Discovery log cleared via web");
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDiscoveryRemove() {
    if (s_stats) {
        String number = server.arg("number");
        if (number.length() > 0) {
            s_stats->removeDiscovery(number.c_str());
            if (s_logger) s_logger->systemLog("Discovery entry removed: %s", number.c_str());
        }
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Audio preview handler --------------------------------------------------

static void handlePreview() {
    String path = server.arg("path");
    if (path.length() == 0) {
        server.send(400, "text/plain", "Missing path");
        return;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
        server.send(404, "text/plain", "File not found");
        return;
    }

    String lower = path;
    lower.toLowerCase();
    const char* mime = lower.endsWith(".wav") ? "audio/wav" : "audio/mpeg";
    server.streamFile(f, mime);
    f.close();
}

// --- Settings persistence ---------------------------------------------------

static const char* SETTINGS_FILE = "/system/settings.json";
static const char* SETTINGS_TMP  = "/system/settings.tmp";

static void loadSettings() {
    if (!s_phone) return;
    // Recover from interrupted save.
    if (!SD.exists(SETTINGS_FILE) && SD.exists(SETTINGS_TMP)) {
        SD.rename(SETTINGS_TMP, SETTINGS_FILE);
    }
    File f = SD.open(SETTINGS_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    if (!doc["volume"].isNull())   s_phone->player().setVolume(doc["volume"].as<uint8_t>());
    if (!doc["digit_gap"].isNull()) s_phone->setNumberCompleteMs(doc["digit_gap"].as<unsigned long>());
    s_wifi_sta = doc["wifi_sta"].as<bool>();
    if (!doc["wifi_ssid"].isNull()) s_sta_ssid = doc["wifi_ssid"].as<const char*>();
    if (!doc["wifi_pass"].isNull()) s_sta_pass = doc["wifi_pass"].as<const char*>();
    if (!doc["ar_min"].isNull() && !doc["ar_max"].isNull()) {
        s_phone->setAutoRingInterval(
            doc["ar_min"].as<unsigned long>(),
            doc["ar_max"].as<unsigned long>());
    }
    if (!doc["ring_max"].isNull()) s_phone->setMaxRingCadences(doc["ring_max"].as<int>());
    if (!doc["rt_min"].isNull() && !doc["rt_max"].isNull())
        s_phone->setRingToneRange(doc["rt_min"].as<int>(), doc["rt_max"].as<int>());
    if (!doc["alert_idle"].isNull()) s_phone->setAlertIdleMinutes(doc["alert_idle"].as<int>());
    if (!doc["coin_override"].isNull()) s_phone->coinBox().setOverride(doc["coin_override"].as<int>());
    if (!doc["bell_freq"].isNull()) s_phone->bell().setRingFreq(doc["bell_freq"].as<int>());
    if (!doc["line_level"].isNull()) s_phone->player().setLineLevel(doc["line_level"].as<uint8_t>());
    if (!doc["line_on"].isNull() && !doc["line_off"].isNull())
        s_phone->line().setThresholds(doc["line_on"].as<int>(), doc["line_off"].as<int>());
    if (!doc["line_cal"].isNull()) s_phone->line().setCalibrated(doc["line_cal"].as<bool>());
    if (!doc["dial_ticks"].isNull()) s_phone->setDialTicks(doc["dial_ticks"].as<bool>());
    Serial.println("[web] settings loaded");
}

static void saveSettings() {
    if (!s_phone) return;
    if (!SD.exists("/system")) SD.mkdir("/system");

    // Write to temp file first, then rename for crash-safe update.
    File f = SD.open(SETTINGS_TMP, FILE_WRITE);
    if (!f) return;

    JsonDocument doc;
    doc["volume"]   = s_phone->player().getVolume();
    doc["digit_gap"] = s_phone->numberCompleteMs();
    doc["wifi_sta"]  = s_wifi_sta;
    doc["wifi_ssid"] = s_sta_ssid;
    doc["wifi_pass"] = s_sta_pass;
    doc["ar_min"]   = s_phone->autoRingMinMs();
    doc["ar_max"]   = s_phone->autoRingMaxMs();
    doc["ring_max"] = s_phone->maxRingCadences();
    doc["rt_min"] = s_phone->ringToneMinSecs();
    doc["rt_max"] = s_phone->ringToneMaxSecs();
    doc["alert_idle"] = s_phone->alertIdleMinutes();
    doc["coin_override"] = s_phone->coinBox().overrideMode();
    doc["bell_freq"] = s_phone->bell().ringFreq();
    doc["line_level"] = s_phone->player().lineLevel();
    doc["line_on"]  = s_phone->line().thresholdOn();
    doc["line_off"] = s_phone->line().thresholdOff();
    doc["line_cal"] = s_phone->line().calibrated();
    doc["dial_ticks"] = s_phone->dialTicks();
    serializeJson(doc, f);
    f.flush();
    f.close();

    SD.remove(SETTINGS_FILE);
    SD.rename(SETTINGS_TMP, SETTINGS_FILE);
}

// --- Reboot handler ---------------------------------------------------------

static void handleReboot() {
    if (s_logger) s_logger->systemLog("Reboot requested via web");
    if (s_stats) s_stats->save();
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
}

// --- Alias API handlers -----------------------------------------------------

static void handleGetAliases() {
    File f = SD.open("/system/aliases.json", FILE_READ);
    if (!f) {
        server.send(200, "application/json", "[]");
        return;
    }
    String content = f.readString();
    f.close();
    server.send(200, "application/json", content);
}

static void handleSaveAliases() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }

    String body = server.arg("plain");
    if (!SD.exists("/system")) SD.mkdir("/system");

    File f = SD.open("/system/aliases.json", FILE_WRITE);
    if (!f) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Cannot write file\"}");
        return;
    }
    f.print(body);
    f.close();

    // Reload aliases in the audio player immediately.
    s_phone->player().loadAliases();
    if (s_logger) s_logger->systemLog("Aliases updated via web");
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- PWA manifest and service worker ----------------------------------------

static const char MANIFEST_JSON[] PROGMEM = R"rawjson(
{
  "name": "K6 GPO Exhibit",
  "short_name": "K6 Exhibit",
  "description": "Control panel for K6 GPO telephone exhibit",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#1a1a1a",
  "theme_color": "#1a1a1a",
  "icons": [{
    "src": "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='20' fill='%23c41e1e'/><text x='50' y='68' text-anchor='middle' font-size='50' font-family='sans-serif' fill='white'>K6</text></svg>",
    "sizes": "any",
    "type": "image/svg+xml",
    "purpose": "any maskable"
  }]
}
)rawjson";

static void handleManifest() {
    server.send_P(200, "application/json", MANIFEST_JSON);
}

// The portal is no longer a PWA. This self-unregistering worker exists only so
// browsers that installed the old caching service worker purge their caches and
// drop the registration on their next visit (otherwise they could keep serving
// the stale cached portal).
static const char SW_JS[] PROGMEM = R"rawjs(
self.addEventListener('install',()=>self.skipWaiting());
self.addEventListener('activate',e=>{e.waitUntil((async()=>{
  const keys=await caches.keys();
  await Promise.all(keys.map(k=>caches.delete(k)));
  await self.registration.unregister();
  const cs=await self.clients.matchAll();
  cs.forEach(c=>c.navigate(c.url));
})());});
)rawjs";

static void handleServiceWorker() {
    server.send_P(200, "application/javascript", SW_JS);
}

// --- Public interface -------------------------------------------------------

void WebManager::begin(Logger& logger, StatsTracker& stats, PhoneController& phone) {
    s_logger = &logger;
    s_stats  = &stats;
    s_phone  = &phone;

    // Load persisted settings first so Wi-Fi comes up in the configured mode.
    loadSettings();

    // Station mode: try to join the configured network. If it doesn't connect
    // within the timeout, fall back to hosting our own AP so the operator can
    // always reach the portal and correct the credentials.
    bool staOk = false;
    if (s_wifi_sta && s_sta_ssid.length()) {
        Serial.printf("[web] joining Wi-Fi \"%s\"...\n", s_sta_ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(s_sta_ssid.c_str(), s_sta_pass.c_str());
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
        staOk = (WiFi.status() == WL_CONNECTED);
    }

    if (staOk) {
        s_ap_active = false;
        Serial.printf("[web] joined \"%s\" — http://%s/\n",
                      s_sta_ssid.c_str(), WiFi.localIP().toString().c_str());
    } else {
        if (s_wifi_sta) Serial.println("[web] Wi-Fi join failed — hosting own AP instead");
        s_ap_active = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
        delay(100);
        Serial.printf("[web] AP \"%s\" started — http://%s/\n",
                      WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    }

    if (MDNS.begin("k6-exhibit")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("[web] mDNS: http://k6-exhibit.local/");
    }

    server.on("/",                HTTP_GET,  handleIndex);
    server.on("/recovery",        HTTP_GET,  handleRecovery);
    server.on("/api/files",       HTTP_GET,  handleFileList);
    server.on("/api/upload",      HTTP_POST, handleUploadComplete, handleUpload);
    server.on("/api/delete",      HTTP_POST, handleDelete);
    server.on("/api/mkdir",       HTTP_POST, handleMkdir);
    server.on("/api/status",      HTTP_GET,  handleStatus);
    server.on("/api/ota",         HTTP_POST, handleOTA, handleOTAUpload);
    server.on("/api/rollback",    HTTP_POST, handleRollback);
    server.on("/api/logs/system", HTTP_GET,  handleLogSystem);
    server.on("/api/logs/calls",  HTTP_GET,  handleLogCalls);
    server.on("/api/logs/clear",  HTTP_POST, handleLogClear);
    server.on("/api/volume",      HTTP_POST, handleVolume);
    server.on("/api/digitgap",    HTTP_POST, handleDigitGap);
    server.on("/api/wifi",        HTTP_POST, handleWifi);
    server.on("/api/bellfreq",    HTTP_POST, handleBellFreq);
    server.on("/api/linelevel",   HTTP_POST, handleLineLevel);
    server.on("/api/autoring",    HTTP_POST, handleAutoRing);
    server.on("/api/ring",        HTTP_POST, handleRingNow);
    server.on("/api/testring",    HTTP_POST, handleTestRing);
    server.on("/api/tone",        HTTP_POST, handleTone);
    server.on("/api/ringcount",  HTTP_POST, handleRingCount);
    server.on("/api/ringtone",   HTTP_POST, handleRingTone);
    server.on("/api/alertidle",  HTTP_POST, handleAlertIdle);
    server.on("/api/coinmode",   HTTP_POST, handleCoinMode);
    server.on("/api/mode",        HTTP_POST, handleToggleMode);
    server.on("/api/terminal",    HTTP_POST, handleTerminal);
    server.on("/api/stats",       HTTP_GET,  handleStats);
    server.on("/api/stats/reset", HTTP_POST, handleStatsReset);
    server.on("/api/diagnostics", HTTP_GET,  handleDiagnostics);
    server.on("/api/diag",        HTTP_GET,  handleDiag);
    server.on("/api/discovery",       HTTP_GET,  handleDiscovery);
    server.on("/api/discovery/clear",  HTTP_POST, handleDiscoveryClear);
    server.on("/api/discovery/remove", HTTP_POST, handleDiscoveryRemove);
    server.on("/api/aliases",     HTTP_GET,  handleGetAliases);
    server.on("/api/aliases",     HTTP_POST, handleSaveAliases);
    server.on("/api/preview",     HTTP_GET,  handlePreview);
    server.on("/api/reboot",      HTTP_POST, handleReboot);
    server.on("/manifest.json",   HTTP_GET,  handleManifest);
    server.on("/sw.js",           HTTP_GET,  handleServiceWorker);

    // Silence the "request handler not found" spam from favicon/OS captive-
    // portal probes: redirect stray GETs to the portal, 404 everything else.
    server.onNotFound([]() {
        if (server.method() == HTTP_GET && !server.uri().startsWith("/api/")) {
            server.sendHeader("Location",
                              String("http://") + currentIP().toString() + "/");
            server.send(302, "text/plain", "redirecting");
        } else {
            server.send(404, "text/plain", "not found");
        }
    });

    server.begin();
    active_ = true;
    if (s_ap_active) logger.systemLog("Wi-Fi AP started SSID=%s", WIFI_AP_SSID);
    else             logger.systemLog("Wi-Fi joined SSID=%s ip=%s",
                                      s_sta_ssid.c_str(), WiFi.localIP().toString().c_str());
    Serial.println("[web] server ready");
}

void WebManager::update() {
    if (active_) server.handleClient();
}

void WebManager::persistSettings() {
    saveSettings();
}

String WebManager::selfTest() {
    return s_phone ? buildSelfTest(*s_phone) : String("phone not ready");
}

String WebManager::audioProbe() {
    return s_phone ? buildProbe(*s_phone) : String("phone not ready");
}
