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

    // Play a single short "tick" in the earpiece — one per rotary dial pulse,
    // to reproduce the clicks a real GPO dial makes as it runs back.  The
    // click WAV is generated once at begin() so this is just a fast reconnect.
    bool playClick();

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

    // Playback position and length of the current file, in seconds. Both are 0
    // for a looping tone (dial, busy, ringing), which has no end to count to.
    uint32_t playPositionSecs();
    uint32_t playDurationSecs();

    // Must be called every loop() to feed the I2S DMA buffers.
    void update();

    // Volume 0-21
    void setVolume(uint8_t vol);
    uint8_t getVolume() const { return volume_; }

    // Master line-level trim (0-100%). Attenuates ALL audio below the
    // library's minimum volume step, so the earpiece level can be set
    // without overdriving it. 100% = no extra attenuation.
    void setLineLevel(uint8_t pct);
    uint8_t lineLevel() const { return line_level_; }

    // 3-band tone control (low / mid / high shelf gains, each -40..+6 dB).
    // Used to band-limit audio to the telephone band, which also avoids
    // low-frequency energy saturating the small coupling transformer.
    void setEq(int8_t lowdB, int8_t middB, int8_t highdB);

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
    uint8_t line_level_ = 100;  // master trim 0-100% (100 = no attenuation)
    String loop_path_;
    unsigned long tone_end_ = 0;  // auto-stop time for test tone (0 = off)

    int  countFilesIn(const char* dir);
    bool fileExists(const char* path);
    bool generateTickFile();  // write the dial-pulse click WAV to SD once
    unsigned long last_sd_check_ = 0;
    static const int MAX_ALIASES = 32;
    struct Alias { char number[12]; char name[32]; };
    Alias aliases_[MAX_ALIASES] = {};
    int alias_count_ = 0;
};
