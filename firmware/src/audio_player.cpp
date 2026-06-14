#include "audio_player.h"
#include "config.h"
#include <ArduinoJson.h>

static const char* ALIASES_FILE = "/system/aliases.json";

bool AudioPlayer::begin() {
    // Initialise SD card on the default VSPI bus.
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("[audio] SD card init failed");
        sd_ok_ = false;
    } else {
        Serial.printf("[audio] SD card ready  type=%d  size=%lluMB\n",
                      SD.cardType(), SD.cardSize() / (1024 * 1024));
        sd_ok_ = true;
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
