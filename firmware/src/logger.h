#pragma once

#include <Arduino.h>

// ============================================================================
// SD card logger — two separate log files
//
//   /logs/system.log  — boot, errors, Wi-Fi, SD, mode changes, OTA
//   /logs/calls.log   — incoming/outgoing calls with timestamps
//
// Logs auto-rotate when they exceed MAX_LOG_SIZE bytes (oldest entries are
// trimmed by truncating the first half of the file).
// ============================================================================

class Logger {
public:
    void begin();

    // System log entries
    void systemLog(const char* fmt, ...);

    // Call log entries
    void callLog(const char* fmt, ...);

    // Read entire log file contents (caller must free() the returned buffer).
    // Returns nullptr if file doesn't exist.  Sets *len to the length.
    char* readSystemLog(size_t* len);
    char* readCallLog(size_t* len);

    // Clear a log file
    void clearSystemLog();
    void clearCallLog();

private:
    void appendLog(const char* path, const char* line);
    void rotateIfNeeded(const char* path);
    void checkDailyRotation();
    String timestamp();

    unsigned long boot_ms_ = 0;
    int last_uptime_day_ = 0;
    uint16_t boot_number_ = 0;
};
