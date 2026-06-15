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

private:
    bool active_ = false;
};
