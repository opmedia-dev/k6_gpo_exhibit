#include "audio_player.h"
#include "config.h"
#include <SPI.h>
#include <ArduinoJson.h>

static const char* ALIASES_FILE = "/system/aliases.json";

bool AudioPlayer::begin() {
    // GPIO 5 (CS) is an ESP32 strapping pin that outputs PWM during boot,
    // which sends spurious chip-select pulses to the SD card.  Explicitly
    // initialise SPI, deselect the card, and send dummy clocks to reset
    // the card's SPI state machine before attempting SD.begin().
    SPI.begin();
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);  // deselect card
    delay(10);
    // Send 80 dummy clocks (10 bytes of 0xFF) with CS high to flush
    // any partial command the card received during boot.
    for (int i = 0; i < 10; i++) SPI.transfer(0xFF);
    delay(10);

    // Try SD init up to 3 times — some cards need a retry after boot glitches.
    sd_ok_ = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (SD.begin(PIN_SD_CS, SPI, 4000000)) {
            Serial.printf("[audio] SD card ready  type=%d  size=%lluMB  (attempt %d)\n",
                          SD.cardType(), SD.cardSize() / (1024 * 1024), attempt);
            sd_ok_ = true;
            break;
        }
        Serial.printf("[audio] SD init attempt %d failed\n", attempt);
        SD.end();
        delay(100);
    }
    if (!sd_ok_) {
        Serial.println("[audio] SD card init failed after 3 attempts");
    }

    // Initialise I2S output.
    audio_.setPinout(PIN_I2S_BCLK, PIN_I2S_LRCLK, PIN_I2S_DOUT);
    audio_.setVolume(15);  // 0-21

    if (sd_ok_) loadAliases();

    return sd_ok_;
}

bool AudioPlayer::playFile(const char* path, bool loop) {
    if (!sd_ok_) return false;
    if (!fileExists(path)) {
        Serial.printf("[audio] file not found: %s\n", path);
        return false;
    }

    looping_   = loop;
    loop_path_ = path;

    bool ok = audio_.connecttoFS(SD, path);
    if (ok) Serial.printf("[audio] playing: %s%s\n", path, loop ? " (loop)" : "");
    return ok;
}

bool AudioPlayer::playDialTone() {
    return playFile(SD_FILE_DIALTONE, true);
}

bool AudioPlayer::playBusyTone() {
    return playFile(SD_FILE_BUSY, true);
}

bool AudioPlayer::playNotRecognised() {
    return playFile(SD_FILE_NOT_REC, false);
}

bool AudioPlayer::playRandomHistory() {
    int count = countFilesIn(SD_DIR_HISTORY);
    if (count <= 0) {
        Serial.println("[audio] no history tracks found");
        return false;
    }

    int pick = random(0, count);
    int idx  = 0;

    File dir = SD.open(SD_DIR_HISTORY);
    if (!dir) return false;

    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
                if (idx == pick) {
                    String fullPath = String(SD_DIR_HISTORY) + "/" + name;
                    entry.close();
                    dir.close();
                    Serial.printf("[audio] random history pick %d/%d: %s\n",
                                  pick + 1, count, fullPath.c_str());
                    return playFile(fullPath.c_str(), false);
                }
                idx++;
            }
        }
        entry.close();
    }
    dir.close();
    return false;
}

String AudioPlayer::resolveAlias(const char* number) {
    for (int i = 0; i < alias_count_; i++) {
        if (strcmp(aliases_[i].number, number) == 0) {
            Serial.printf("[audio] alias: %s → %s\n", number, aliases_[i].name);
            return String(aliases_[i].name);
        }
    }
    return String(number);
}

bool AudioPlayer::playForNumber(const char* number) {
    String resolved = resolveAlias(number);
    char path[64];
    snprintf(path, sizeof(path), "%s/%s.mp3", SD_DIR_NUMBERS, resolved.c_str());

    if (fileExists(path)) {
        Serial.printf("[audio] number match: %s\n", path);
        return playFile(path, false);
    }

    Serial.printf("[audio] no match for number '%s'\n", number);
    return playNotRecognised();
}

void AudioPlayer::loadAliases() {
    File f = SD.open(ALIASES_FILE, FILE_READ);
    if (!f) {
        Serial.println("[audio] no aliases file");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[audio] alias parse error: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    alias_count_ = 0;
    for (JsonObject obj : arr) {
        if (alias_count_ >= MAX_ALIASES) break;
        strncpy(aliases_[alias_count_].number, obj["number"] | "", 11);
        aliases_[alias_count_].number[11] = '\0';
        strncpy(aliases_[alias_count_].name, obj["name"] | "", 31);
        aliases_[alias_count_].name[31] = '\0';
        alias_count_++;
    }
    Serial.printf("[audio] loaded %d aliases\n", alias_count_);
}

void AudioPlayer::stop() {
    audio_.stopSong();
    looping_ = false;
    loop_path_ = "";
}

bool AudioPlayer::isPlaying() {
    return audio_.isRunning();
}

void AudioPlayer::update() {
    audio_.loop();

    // Handle looping: restart file when playback finishes.
    if (looping_ && !audio_.isRunning() && loop_path_.length() > 0) {
        audio_.connecttoFS(SD, loop_path_.c_str());
    }
}

void AudioPlayer::setVolume(uint8_t vol) {
    volume_ = vol;
    audio_.setVolume(vol);
}

int AudioPlayer::countFilesIn(const char* dirPath) {
    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) return 0;

    int count = 0;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
                count++;
            }
        }
        entry.close();
    }
    dir.close();
    return count;
}

bool AudioPlayer::fileExists(const char* path) {
    return SD.exists(path);
}

bool AudioPlayer::checkSdCard() {
    if (millis() - last_sd_check_ < 10000) return sd_ok_;
    last_sd_check_ = millis();

    if (sd_ok_) {
        // Quick health check: try to open root.
        File root = SD.open("/");
        if (!root) {
            Serial.println("[audio] SD card lost — attempting remount");
            sd_ok_ = false;
        } else {
            root.close();
            return true;
        }
    }

    // Attempt remount.
    SD.end();
    if (SD.begin(PIN_SD_CS)) {
        Serial.println("[audio] SD card remounted OK");
        sd_ok_ = true;
        loadAliases();
    }
    return sd_ok_;
}
