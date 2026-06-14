#pragma once

#include <Arduino.h>
#include "logger.h"

// ============================================================================
// Wi-Fi AP + Web file manager + OTA firmware update + Log viewer
//
// The ESP32 creates a Wi-Fi access point ("K6-Exhibit") and serves a web
// interface for SD card file management, firmware updates, and log viewing.
//
// Endpoints:
//   GET  /              HTML file manager UI
//   GET  /api/files     JSON directory listing (?path=/dir)
//   POST /api/upload    Multipart file upload (?path=/dir)
//   POST /api/delete    Delete a file (?path=/file)
//   POST /api/mkdir     Create a directory (?path=/dir)
//   GET  /api/status    System status JSON
//   POST /api/ota       Firmware binary upload (OTA update)
//   GET  /api/logs/system   System log contents
//   GET  /api/logs/calls    Call log contents
//   POST /api/logs/clear    Clear logs (?log=system|calls|all)
// ============================================================================

class WebManager {
public:
    void begin(Logger& logger);
    void update();

    bool isActive() const { return active_; }

private:
    bool active_ = false;
};
