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

bool AudioPlayer::playTestTone(int hz, int secs) {
    if (!sd_ok_) return false;
    if (hz < 50)   hz = 50;
    if (hz > 4000) hz = 4000;
    if (secs < 1)  secs = 1;
    if (secs > 60) secs = 60;

    const uint32_t sampleRate = 16000;
    // Whole number of periods so the looped WAV joins seamlessly (no click).
    uint32_t periodSamples = sampleRate / hz;      // samples per cycle
    if (periodSamples < 1) periodSamples = 1;
    uint32_t cycles        = (sampleRate / 2) / periodSamples;  // ~0.5s buffer
    if (cycles < 1) cycles = 1;
    uint32_t numSamples    = periodSamples * cycles;
    uint32_t dataBytes     = numSamples * 2;       // 16-bit mono

    const char* path = "/system/_testtone.wav";
    if (!SD.exists("/system")) SD.mkdir("/system");
    SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.println("[audio] could not create test tone file");
        return false;
    }

    // --- WAV header (44 bytes, PCM 16-bit mono) ---
    auto w32 = [&](uint32_t v){ uint8_t b[4]={(uint8_t)v,(uint8_t)(v>>8),(uint8_t)(v>>16),(uint8_t)(v>>24)}; f.write(b,4); };
    auto w16 = [&](uint16_t v){ uint8_t b[2]={(uint8_t)v,(uint8_t)(v>>8)}; f.write(b,2); };
    f.write((const uint8_t*)"RIFF", 4);  w32(36 + dataBytes);
    f.write((const uint8_t*)"WAVE", 4);
    f.write((const uint8_t*)"fmt ", 4);  w32(16);
    w16(1);                    // PCM
    w16(1);                    // channels
    w32(sampleRate);
    w32(sampleRate * 2);       // byte rate
    w16(2);                    // block align
    w16(16);                   // bits per sample
    f.write((const uint8_t*)"data", 4);  w32(dataBytes);

    // --- Sine samples ---
    const float amp = 0.6f * 32767.0f;
    uint8_t buf[512];
    int bi = 0;
    for (uint32_t i = 0; i < numSamples; i++) {
        float phase = 2.0f * PI * (float)(i % periodSamples) / (float)periodSamples;
        int16_t s = (int16_t)(amp * sinf(phase));
        buf[bi++] = (uint8_t)s;
        buf[bi++] = (uint8_t)(s >> 8);
        if (bi >= (int)sizeof(buf)) { f.write(buf, bi); bi = 0; }
    }
    if (bi) f.write(buf, bi);
    f.close();

    bool ok = playFile(path, true);  // loop the buffer
    if (ok) {
        tone_end_ = millis() + (unsigned long)secs * 1000;
        Serial.printf("[audio] test tone %d Hz for %ds\n", hz, secs);
    }
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
    tone_end_ = 0;
}

bool AudioPlayer::isPlaying() {
    return audio_.isRunning();
}

void AudioPlayer::update() {
    audio_.loop();

    // Auto-stop the test tone after its duration.
    if (tone_end_ && millis() >= tone_end_) {
        tone_end_ = 0;
        stop();
        Serial.println("[audio] test tone finished");
        return;
    }

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
