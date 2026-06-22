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
    uint32_t avgCallSeconds() const;

    // Most dialled numbers (top 5). Returns count of entries filled.
    struct NumberEntry { char number[12]; uint16_t count; };
    int topNumbers(NumberEntry* out, int maxEntries) const;

    // Discovery log — unrecognised numbers visitors tried to dial.
    int discoveryCount() const { return disc_count_; }
    const NumberEntry* discoveryEntries() const { return discovery_; }
    void clearDiscovery();
    void removeDiscovery(const char* number);

    // Save stats to SD now (also called periodically by update()).
    void save();

private:
    void load();

    CallStats stats_ = {};
    unsigned long last_save_ms_ = 0;
    unsigned long boot_time_ms_ = 0;
    bool dirty_ = false;

    // Track up to 32 unique dialled numbers.
    static const int MAX_NUMBERS = 32;
    NumberEntry numbers_[MAX_NUMBERS] = {};
    int num_count_ = 0;

    void incrementNumber(const char* number);
    void incrementDiscovery(const char* number);
    void saveDiscovery();
    void loadDiscovery();

    unsigned long call_start_ms_ = 0;
    bool in_call_ = false;

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
