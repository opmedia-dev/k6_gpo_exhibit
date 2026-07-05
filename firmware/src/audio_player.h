#pragma once

#include <Arduino.h>
#include "Audio.h"   // ESP32-audioI2S
#include <SD.h>

// ============================================================================
// Audio player — SD-card MP3 playback over I2S
//
// Wraps the ESP32-audioI2S library.  All audio (MP3 files and generated
// tones) goes out through the I2S bus to a MAX98357A DAC module, then
// through a coupling transformer to the phone line.
//
// SD card directory layout:
//   /system/dialtone.mp3        continuous dial tone loop
//   /system/busy.mp3            busy tone loop
//   /system/not_recognised.mp3  "number not recognised" announcement
//   /history/NNN.mp3            exhibit history tracks (random pick)
//   /numbers/<digits>.mp3       tracks keyed by dialled number
// ============================================================================

class AudioPlayer {
public:
    bool begin();

    // Play a file from SD card.  If loop is true the file restarts on EOF.
    bool playFile(const char* path, bool loop = false);

    // Generate and play a steady sine test tone for a fixed duration.
    // Writes a short looping WAV to the SD card and plays it looped, then
    // auto-stops after `secs`.  Useful for measuring the line output.
    bool playTestTone(int hz, int secs);

    // Play the built-in dial tone (file-based).
    bool playDialTone();

    // Play the busy tone.
    bool playBusyTone();

    // Play the "not recognised" announcement.
    bool playNotRecognised();

    // Pick a random history track and play it.
    bool playRandomHistory();

    // Look up a dialled number and play the matching file, or play
    // "not recognised" if no match exists.  Checks aliases first.
    bool playForNumber(const char* number);

    // Resolve a dialled number through the alias table.
    // Returns the alias filename (without path/extension) if found,
    // or the original number if no alias exists.
    String resolveAlias(const char* number);

    void stop();
    bool isPlaying();

    // Must be called every loop() to feed the I2S DMA buffers.
    void update();

    // Volume 0-21
    void setVolume(uint8_t vol);
    uint8_t getVolume() const { return volume_; }

    // Currently playing file path (empty if not playing).
    const String& currentFile() const { return loop_path_; }

    bool sdReady() const { return sd_ok_; }

    // Attempt SD card remount if it was lost. Returns true if card is now OK.
    bool checkSdCard();

    // Reload aliases from /system/aliases.json (call after editing aliases).
    void loadAliases();

private:
    Audio  audio_;
    bool   sd_ok_     = false;
    bool   looping_   = false;
    uint8_t volume_   = 15;
    String loop_path_;
    unsigned long tone_end_ = 0;  // auto-stop time for test tone (0 = off)

    int  countFilesIn(const char* dir);
    bool fileExists(const char* path);
    unsigned long last_sd_check_ = 0;
    static const int MAX_ALIASES = 32;
    struct Alias { char number[12]; char name[32]; };
    Alias aliases_[MAX_ALIASES] = {};
    int alias_count_ = 0;
};
