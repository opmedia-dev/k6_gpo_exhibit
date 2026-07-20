#include "stats.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>

static const char* STATS_FILE = "/logs/stats.json";
static const char* STATS_TMP  = "/logs/stats.tmp";
static const char* DISC_FILE  = "/logs/discovery.json";
static const char* DISC_TMP   = "/logs/discovery.tmp";
static const char* DIAG_FILE  = "/logs/diag.json";
static const char* DIAG_TMP   = "/logs/diag.tmp";
static const unsigned long SAVE_INTERVAL_MS = 60000;  // save every 60s if dirty

void StatsTracker::begin() {
    boot_time_ms_ = millis();
    session_ = {};
    session_num_count_ = 0;
    memset(session_numbers_, 0, sizeof(session_numbers_));
    load();
    loadDiscovery();
    loadDiag();
}

void StatsTracker::update() {
    // Accumulate uptime.
    stats_.uptime_seconds = stats_.uptime_seconds;  // loaded value is base

    if (dirty_ && millis() - last_save_ms_ > SAVE_INTERVAL_MS) {
        save();
    }
}

void StatsTracker::recordIncomingRing() {
    stats_.total_incoming++;
    session_.incoming++;
    dirty_ = true;
}

void StatsTracker::recordIncomingAnswered() {
    stats_.total_answered++;
    session_.answered++;
    dirty_ = true;
}

void StatsTracker::recordOutgoingCall(const char* number) {
    stats_.total_outgoing++;
    session_.outgoing++;
    incrementNumber(number);
    incrementSessionNumber(number);
    dirty_ = true;
}

void StatsTracker::recordNotRecognised(const char* number) {
    stats_.total_not_recognised++;
    session_.not_recognised++;
    incrementNumber(number);
    incrementSessionNumber(number);
    dirty_ = true;
}

void StatsTracker::recordDiscovery(const char* number) {
    incrementDiscovery(number);
}

void StatsTracker::recordCoinCollected() {
    stats_.total_coin_collected++;
    dirty_ = true;
}

void StatsTracker::recordCoinRefunded() {
    stats_.total_coin_refunded++;
    dirty_ = true;
}

void StatsTracker::recordPickup() {
    stats_.total_pickups++;
    session_.pickups++;
    dirty_ = true;
}

void StatsTracker::recordFirstDigit(unsigned long dialToneMs) {
    stats_.total_first_digit_ms += dialToneMs;
    stats_.first_digit_count++;
    dirty_ = true;
}

void StatsTracker::recordCompletion() {
    stats_.total_completions++;
    session_.completions++;
    dirty_ = true;
}

void StatsTracker::recordError(ErrorType type, const char* detail) {
    auto& e = errors_[err_write_];
    e.timestamp = millis() / 1000;
    e.type = type;
    if (detail) {
        strncpy(e.detail, detail, sizeof(e.detail) - 1);
        e.detail[sizeof(e.detail) - 1] = '\0';
    } else {
        e.detail[0] = '\0';
    }
    err_write_ = (err_write_ + 1) % MAX_ERRORS;
    if (err_count_ < MAX_ERRORS) err_count_++;
}

void StatsTracker::callStarted() {
    if (!in_call_) {
        in_call_ = true;
        call_start_ms_ = millis();
    }
}

void StatsTracker::callEnded() {
    if (in_call_) {
        in_call_ = false;
        uint32_t duration = (millis() - call_start_ms_) / 1000;
        stats_.total_call_seconds += duration;
        stats_.call_count++;
        if (duration > stats_.longest_call_seconds) {
            stats_.longest_call_seconds = duration;
        }
        dirty_ = true;
        save();  // flush immediately on call end for power-off safety
    }
}

uint32_t StatsTracker::avgCallSeconds() const {
    return stats_.call_count > 0 ? stats_.total_call_seconds / stats_.call_count : 0;
}

void StatsTracker::incrementNumber(const char* number) {
    // Find existing entry.
    for (int i = 0; i < num_count_; i++) {
        if (strcmp(numbers_[i].number, number) == 0) {
            numbers_[i].count++;
            return;
        }
    }
    // Add new entry if room.
    if (num_count_ < MAX_NUMBERS) {
        strncpy(numbers_[num_count_].number, number, 11);
        numbers_[num_count_].number[11] = '\0';
        numbers_[num_count_].count = 1;
        num_count_++;
    }
}

void StatsTracker::incrementSessionNumber(const char* number) {
    for (int i = 0; i < session_num_count_; i++) {
        if (strcmp(session_numbers_[i].number, number) == 0) {
            session_numbers_[i].count++;
            return;
        }
    }
    if (session_num_count_ < MAX_NUMBERS) {
        strncpy(session_numbers_[session_num_count_].number, number, 11);
        session_numbers_[session_num_count_].number[11] = '\0';
        session_numbers_[session_num_count_].count = 1;
        session_num_count_++;
    }
}

// Sort a copy of `src` (count elements) by count descending into `out`.
static int topOf(const StatsTracker::NumberEntry* src, int count,
                 StatsTracker::NumberEntry* out, int maxEntries) {
    StatsTracker::NumberEntry sorted[32];
    if (count > 32) count = 32;
    memcpy(sorted, src, sizeof(StatsTracker::NumberEntry) * count);

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (sorted[j].count > sorted[i].count) {
                StatsTracker::NumberEntry tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    int n = count < maxEntries ? count : maxEntries;
    memcpy(out, sorted, sizeof(StatsTracker::NumberEntry) * n);
    return n;
}

int StatsTracker::topNumbers(NumberEntry* out, int maxEntries) const {
    return topOf(numbers_, num_count_, out, maxEntries);
}

int StatsTracker::topSessionNumbers(NumberEntry* out, int maxEntries) const {
    return topOf(session_numbers_, session_num_count_, out, maxEntries);
}

void StatsTracker::save() {
    // Update cumulative uptime before saving.
    unsigned long session_secs = (millis() - boot_time_ms_) / 1000;

    if (!SD.exists("/logs")) SD.mkdir("/logs");

    // Write to temp file first, then rename for crash-safe update.
    File f = SD.open(STATS_TMP, FILE_WRITE);
    if (!f) return;

    JsonDocument doc;
    doc["incoming"]        = stats_.total_incoming;
    doc["outgoing"]        = stats_.total_outgoing;
    doc["answered"]        = stats_.total_answered;
    doc["not_recognised"]  = stats_.total_not_recognised;
    doc["coin_collected"]  = stats_.total_coin_collected;
    doc["coin_refunded"]   = stats_.total_coin_refunded;
    doc["uptime"]          = stats_.uptime_seconds + session_secs;
    doc["call_seconds"]     = stats_.total_call_seconds;
    doc["longest_call"]     = stats_.longest_call_seconds;
    doc["call_count"]       = stats_.call_count;
    doc["pickups"]          = stats_.total_pickups;
    doc["completions"]      = stats_.total_completions;
    doc["first_digit_ms"]   = stats_.total_first_digit_ms;
    doc["first_digit_n"]    = stats_.first_digit_count;

    JsonArray nums = doc["numbers"].to<JsonArray>();
    for (int i = 0; i < num_count_; i++) {
        JsonObject obj = nums.add<JsonObject>();
        obj["n"] = numbers_[i].number;
        obj["c"] = numbers_[i].count;
    }

    serializeJson(doc, f);
    f.flush();
    f.close();

    SD.remove(STATS_FILE);
    SD.rename(STATS_TMP, STATS_FILE);

    last_save_ms_ = millis();
    dirty_ = false;
    Serial.println("[stats] saved");
}

void StatsTracker::load() {
    // If previous save was interrupted, recover from temp file.
    if (!SD.exists(STATS_FILE) && SD.exists(STATS_TMP)) {
        SD.rename(STATS_TMP, STATS_FILE);
    }
    File f = SD.open(STATS_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[stats] parse error: %s\n", err.c_str());
        return;
    }

    stats_.total_incoming       = doc["incoming"]       | 0;
    stats_.total_outgoing       = doc["outgoing"]       | 0;
    stats_.total_answered       = doc["answered"]        | 0;
    stats_.total_not_recognised = doc["not_recognised"]  | 0;
    stats_.total_coin_collected = doc["coin_collected"]  | 0;
    stats_.total_coin_refunded  = doc["coin_refunded"]   | 0;
    stats_.uptime_seconds       = doc["uptime"]          | 0;
    stats_.total_call_seconds    = doc["call_seconds"]     | 0;
    stats_.longest_call_seconds  = doc["longest_call"]     | 0;
    stats_.call_count            = doc["call_count"]       | 0;
    stats_.total_pickups         = doc["pickups"]          | 0;
    stats_.total_completions     = doc["completions"]      | 0;
    stats_.total_first_digit_ms  = doc["first_digit_ms"]   | 0;
    stats_.first_digit_count     = doc["first_digit_n"]    | 0;

    JsonArray nums = doc["numbers"].as<JsonArray>();
    num_count_ = 0;
    for (JsonObject obj : nums) {
        if (num_count_ >= MAX_NUMBERS) break;
        strncpy(numbers_[num_count_].number, obj["n"] | "", 11);
        numbers_[num_count_].number[11] = '\0';
        numbers_[num_count_].count = obj["c"] | 0;
        num_count_++;
    }

    Serial.printf("[stats] loaded: %u incoming, %u outgoing, %u answered\n",
                  stats_.total_incoming, stats_.total_outgoing, stats_.total_answered);
}

// --- Discovery log persistence -----------------------------------------------

void StatsTracker::incrementDiscovery(const char* number) {
    for (int i = 0; i < disc_count_; i++) {
        if (strcmp(discovery_[i].number, number) == 0) {
            discovery_[i].count++;
            disc_dirty_ = true;
            saveDiscovery();
            return;
        }
    }
    if (disc_count_ < MAX_DISCOVERY) {
        strncpy(discovery_[disc_count_].number, number, 11);
        discovery_[disc_count_].number[11] = '\0';
        discovery_[disc_count_].count = 1;
        disc_count_++;
        disc_dirty_ = true;
        saveDiscovery();
    }
}

void StatsTracker::clearDiscovery() {
    disc_count_ = 0;
    memset(discovery_, 0, sizeof(discovery_));
    SD.remove(DISC_FILE);
    SD.remove(DISC_TMP);
    Serial.println("[stats] discovery log cleared");
}

void StatsTracker::removeDiscovery(const char* number) {
    for (int i = 0; i < disc_count_; i++) {
        if (strcmp(discovery_[i].number, number) == 0) {
            for (int j = i; j < disc_count_ - 1; j++) {
                discovery_[j] = discovery_[j + 1];
            }
            disc_count_--;
            saveDiscovery();
            return;
        }
    }
}

void StatsTracker::saveDiscovery() {
    if (!SD.exists("/logs")) SD.mkdir("/logs");

    File f = SD.open(DISC_TMP, FILE_WRITE);
    if (!f) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < disc_count_; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["n"] = discovery_[i].number;
        obj["c"] = discovery_[i].count;
    }

    serializeJson(doc, f);
    f.flush();
    f.close();

    SD.remove(DISC_FILE);
    SD.rename(DISC_TMP, DISC_FILE);
    disc_dirty_ = false;
    Serial.println("[stats] discovery saved");
}

void StatsTracker::loadDiscovery() {
    if (!SD.exists(DISC_FILE) && SD.exists(DISC_TMP)) {
        SD.rename(DISC_TMP, DISC_FILE);
    }
    File f = SD.open(DISC_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[stats] discovery parse error: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    disc_count_ = 0;
    for (JsonObject obj : arr) {
        if (disc_count_ >= MAX_DISCOVERY) break;
        strncpy(discovery_[disc_count_].number, obj["n"] | "", 11);
        discovery_[disc_count_].number[11] = '\0';
        discovery_[disc_count_].count = obj["c"] | 0;
        disc_count_++;
    }
    Serial.printf("[stats] discovery loaded: %d entries\n", disc_count_);
}

// --- Persisted diagnostics (drift tracking) ----------------------------------

void StatsTracker::recordBootLine(uint16_t bootNumber, int raw) {
    if (bootline_count_ >= MAX_BOOTLINES) {
        // Drop the oldest entry to make room.
        for (int i = 0; i < MAX_BOOTLINES - 1; i++) bootlines_[i] = bootlines_[i + 1];
        bootline_count_ = MAX_BOOTLINES - 1;
    }
    bootlines_[bootline_count_].boot = bootNumber;
    bootlines_[bootline_count_].raw  = (int16_t)raw;
    bootline_count_++;
    saveDiag();
}

void StatsTracker::recordSelfTest(const char* text, uint32_t uptimeSecs) {
    if (text) {
        strncpy(last_selftest_, text, sizeof(last_selftest_) - 1);
        last_selftest_[sizeof(last_selftest_) - 1] = '\0';
    } else {
        last_selftest_[0] = '\0';
    }
    last_selftest_uptime_ = uptimeSecs;
    saveDiag();
}

void StatsTracker::saveDiag() {
    if (!SD.exists("/logs")) SD.mkdir("/logs");

    File f = SD.open(DIAG_TMP, FILE_WRITE);
    if (!f) return;

    JsonDocument doc;
    JsonArray arr = doc["boot_lines"].to<JsonArray>();
    for (int i = 0; i < bootline_count_; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["b"] = bootlines_[i].boot;
        obj["r"] = bootlines_[i].raw;
    }
    doc["selftest"]        = last_selftest_;
    doc["selftest_uptime"] = last_selftest_uptime_;

    serializeJson(doc, f);
    f.flush();
    f.close();

    SD.remove(DIAG_FILE);
    SD.rename(DIAG_TMP, DIAG_FILE);
}

void StatsTracker::loadDiag() {
    if (!SD.exists(DIAG_FILE) && SD.exists(DIAG_TMP)) {
        SD.rename(DIAG_TMP, DIAG_FILE);
    }
    File f = SD.open(DIAG_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[stats] diag parse error: %s\n", err.c_str());
        return;
    }

    bootline_count_ = 0;
    JsonArray arr = doc["boot_lines"].as<JsonArray>();
    for (JsonObject obj : arr) {
        if (bootline_count_ >= MAX_BOOTLINES) break;
        bootlines_[bootline_count_].boot = obj["b"] | 0;
        bootlines_[bootline_count_].raw  = obj["r"] | 0;
        bootline_count_++;
    }
    strncpy(last_selftest_, doc["selftest"] | "", sizeof(last_selftest_) - 1);
    last_selftest_[sizeof(last_selftest_) - 1] = '\0';
    last_selftest_uptime_ = doc["selftest_uptime"] | 0;
    Serial.printf("[stats] diag loaded: %d boot-line samples\n", bootline_count_);
}
