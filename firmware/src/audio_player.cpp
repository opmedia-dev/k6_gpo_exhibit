#include "audio_player.h"
#include "config.h"

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

bool AudioPlayer::playForNumber(const char* number) {
    char path[64];
    snprintf(path, sizeof(path), "%s/%s.mp3", SD_DIR_NUMBERS, number);

    if (fileExists(path)) {
        Serial.printf("[audio] number match: %s\n", path);
        return playFile(path, false);
    }

    Serial.printf("[audio] no match for number '%s'\n", number);
    return playNotRecognised();
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
