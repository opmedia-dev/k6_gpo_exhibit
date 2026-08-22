#pragma once

#include <Arduino.h>
#include "logger.h"
#include "stats.h"

class PhoneController;

// ============================================================================
// Wi-Fi AP + Web file manager + OTA firmware update + Log viewer + Controls
//
// The ESP32 creates a Wi-Fi access point ("K6-Exhibit") and serves a web
// interface for SD card file management, firmware updates, log viewing,
// volume control, auto-ring configuration, and visitor statistics.
// ============================================================================

class WebManager {
public:
    void begin(Logger& logger, StatsTracker& stats, PhoneController& phone);
    void update();

    bool isActive() const { return active_; }

    // Persist current runtime settings to SD (settings.json). Used by the
    // serial calibration command so tuned thresholds survive a reboot.
    void persistSettings();

    // Clear the loop/connection timing counters reported by /api/status so a
    // measurement can be taken over a known window.
    void resetTimingStats();

    // Diagnostics shared with the web terminal, for use from the serial console.
    String selfTest();
    String audioProbe();

    // Link state, timing counters and a 2.4 GHz channel survey as plain text.
    // Scanning takes the radio off our channel for a second or so.
    String wifiReport();

    // Log every request with the time spent handling it. Serial only.
    bool toggleWebTrace();

    // Persist a new AP channel. Takes effect on the next boot, so the caller
    // restarts. Returns false for a channel outside 1-13.
    bool setApChannel(uint8_t channel);

private:
    bool active_ = false;
};
