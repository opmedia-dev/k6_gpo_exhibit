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
};

class StatsTracker {
public:
    void begin();
    void update();

    void recordIncomingRing();
    void recordIncomingAnswered();
    void recordOutgoingCall(const char* number);
    void recordNotRecognised(const char* number);
    void recordCoinCollected();
    void recordCoinRefunded();

    const CallStats& stats() const { return stats_; }

    // Most dialled numbers (top 5). Returns count of entries filled.
    struct NumberEntry { char number[12]; uint16_t count; };
    int topNumbers(NumberEntry* out, int maxEntries) const;

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
};
