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

    // Play the built-in dial tone (file-based).
    bool playDialTone();

    // Play the busy tone.
    bool playBusyTone();

    // Play the "not recognised" announcement.
    bool playNotRecognised();

    // Pick a random history track and play it.
    bool playRandomHistory();

    // Look up a dialled number and play the matching file, or play
    // "not recognised" if no match exists.
    bool playForNumber(const char* number);

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

private:
    Audio  audio_;
    bool   sd_ok_     = false;
    bool   looping_   = false;
    uint8_t volume_   = 15;
    String loop_path_;

    int  countFilesIn(const char* dir);
    bool fileExists(const char* path);
};
