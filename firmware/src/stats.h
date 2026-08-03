#pragma once

#include <Arduino.h>

// ============================================================================
// Visitor / call statistics — persisted to SD card
//
// Tracks call counts, popular numbers, and hourly activity.
// Stats are saved to /logs/stats.json on SD card periodically.
// ============================================================================

struct CallStats {
    uint32_t total_incoming;
    uint32_t total_outgoing;
    uint32_t total_answered;
    uint32_t total_not_recognised;
    uint32_t total_coin_collected;   // Button A presses
    uint32_t total_coin_refunded;    // Button B presses
    uint32_t uptime_seconds;         // cumulative across boots
    uint32_t total_call_seconds;     // total time visitors spent on calls
    uint32_t longest_call_seconds;   // single longest call
    uint32_t call_count;             // completed calls (for average)

    // New engagement metrics
    uint32_t total_pickups;          // total handset lifts (off-hook events)
    uint32_t total_completions;      // sessions where a full number was dialled
    uint32_t total_first_digit_ms;   // sum of time-to-first-digit (ms)
    uint32_t first_digit_count;      // number of sessions that had a first digit
};

// Since-boot activity. Not persisted — resets every power-on. The exhibit is
// hard-powered down out of hours, so this is effectively a "today" summary.
struct SessionStats {
    uint32_t incoming;
    uint32_t outgoing;
    uint32_t answered;
    uint32_t not_recognised;
    uint32_t pickups;
    uint32_t completions;
};

class StatsTracker {
public:
    void begin();
    void update();

    void recordIncomingRing();
    void recordIncomingAnswered();
    void recordOutgoingCall(const char* number);
    void recordNotRecognised(const char* number);
    void recordDiscovery(const char* number);
    void recordCoinCollected();
    void recordCoinRefunded();

    // Engagement metrics.
    void recordPickup();                  // handset lifted
    void recordFirstDigit(unsigned long dialToneMs);  // first digit after dial tone
    void recordCompletion();              // full number dialled

    // Call duration tracking — call from onState when entering/leaving call.
    void callStarted();
    void callEnded();

    // Error tracking.
    enum class ErrorType : uint8_t { BELL_FAULT, LINE_ANOMALY, SD_FAILURE };
    void recordError(ErrorType type, const char* detail = nullptr);
    struct ErrorEntry { unsigned long timestamp; ErrorType type; char detail[48]; };
    static const int MAX_ERRORS = 30;
    int errorCount() const { return err_count_; }
    const ErrorEntry* errorEntries() const { return errors_; }

    const CallStats& stats() const { return stats_; }
    const SessionStats& session() const { return session_; }
    uint32_t avgCallSeconds() const;

    // Most dialled numbers (top 5). Returns count of entries filled.
    struct NumberEntry { char number[12]; uint16_t count; };
    int topNumbers(NumberEntry* out, int maxEntries) const;
    // Same, but only counts numbers dialled since power-on ("today").
    int topSessionNumbers(NumberEntry* out, int maxEntries) const;

    // --- Persisted diagnostics (drift tracking) ------------------------------
    // Record the on-hook line-sense reading captured at boot, tagged with the
    // boot number, so line-level drift is visible across power cycles.
    struct BootLineEntry { uint16_t boot; int16_t raw; };
    void recordBootLine(uint16_t bootNumber, int raw);
    int bootLineCount() const { return bootline_count_; }
    const BootLineEntry* bootLineEntries() const { return bootlines_; }

    // Store the most recent self-test checklist and when it ran (uptime secs).
    void recordSelfTest(const char* text, uint32_t uptimeSecs);
    const char* lastSelfTest() const { return last_selftest_; }
    uint32_t lastSelfTestUptime() const { return last_selftest_uptime_; }

    // Discovery log — unrecognised numbers visitors tried to dial.
    int discoveryCount() const { return disc_count_; }
    const NumberEntry* discoveryEntries() const { return discovery_; }
    void clearDiscovery();
    void removeDiscovery(const char* number);

    // Save stats to SD now (also called periodically by update()).
    void save();

    // Erase all visitor statistics: zero the cumulative + session counters and
    // most-dialled lists in RAM and persist the cleared state to SD.
    void resetStats();

private:
    void load();

    CallStats stats_ = {};
    SessionStats session_ = {};
    unsigned long last_save_ms_ = 0;
    unsigned long boot_time_ms_ = 0;
    bool dirty_ = false;

    // Track up to 32 unique dialled numbers.
    static const int MAX_NUMBERS = 32;
    NumberEntry numbers_[MAX_NUMBERS] = {};
    int num_count_ = 0;

    // Same tracking for the current session only (reset each boot).
    NumberEntry session_numbers_[MAX_NUMBERS] = {};
    int session_num_count_ = 0;

    void incrementNumber(const char* number);
    void incrementSessionNumber(const char* number);
    void incrementDiscovery(const char* number);
    void saveDiscovery();
    void loadDiscovery();

    // --- Persisted diagnostics ----------------------------------------------
    static const int MAX_BOOTLINES = 20;
    BootLineEntry bootlines_[MAX_BOOTLINES] = {};
    int  bootline_count_ = 0;
    char last_selftest_[512] = {};
    uint32_t last_selftest_uptime_ = 0;
    void saveDiag();
    void loadDiag();

    unsigned long call_start_ms_ = 0;
    bool in_call_ = false;
    bool completed_session_ = false;  // completion already counted this pickup

    // Discovery log — numbers visitors dialled that weren't recognised.
    static const int MAX_DISCOVERY = 50;
    NumberEntry discovery_[MAX_DISCOVERY] = {};
    int disc_count_ = 0;
    bool disc_dirty_ = false;

    // Error log (ring buffer, not persisted — resets on reboot).
    ErrorEntry errors_[MAX_ERRORS] = {};
    int err_count_ = 0;
    int err_write_  = 0;
};
