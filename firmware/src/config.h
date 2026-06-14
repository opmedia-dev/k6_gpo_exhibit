#pragma once

// ============================================================================
// K6 GPO Exhibit — ESP32 Interface for GPO 232/332 Telephones
// Pin assignments and system configuration
// ============================================================================

// --- Phone line interface ---------------------------------------------------
constexpr int PIN_LINE_SENSE   = 34;  // ADC1_CH6, input-only GPIO

// --- Bell / ring generator --------------------------------------------------
constexpr int PIN_RING_EN      = 4;   // H-bridge enable (active-high)
constexpr int PIN_RING_A       = 16;  // H-bridge input A
constexpr int PIN_RING_B       = 17;  // H-bridge input B

// --- I2S audio output (to MAX98357A DAC → coupling transformer → phone) -----
constexpr int PIN_I2S_BCLK     = 26;
constexpr int PIN_I2S_LRCLK    = 25;
constexpr int PIN_I2S_DOUT     = 22;

// --- SD card (SPI) ----------------------------------------------------------
constexpr int PIN_SD_CS        = 5;
// MOSI=23, MISO=19, SCK=18 (default VSPI)

// --- External control panel buttons (active-low with internal pull-up) ------
constexpr int PIN_BTN_RING     = 32;  // trigger incoming ring
constexpr int PIN_BTN_CANCEL   = 33;  // cancel ring / stop playback
constexpr int PIN_BTN_RESET    = 27;  // system reset
constexpr int PIN_BTN_MODE     = 14;  // toggle auto/manual ring mode

// --- A+B Coin Box (optional daughter board) ----------------------------------
// These input-only GPIOs are active-low via optocoupler.  When no daughter
// board is installed, the pins float high and the coin box feature is
// automatically disabled.
constexpr int PIN_COIN_SENSE   = 36;  // coin weight switch (coins inserted)
constexpr int PIN_COIN_BTN_A   = 39;  // Button A (collect coins, connect call)
constexpr int PIN_COIN_BTN_B   = 35;  // Button B (refund coins)

// --- Auto-mode indicator lamp ------------------------------------------------
constexpr int PIN_AUTO_LAMP    = 13;  // drives 3-6V panel lamp (active-high)

// --- Status LED -------------------------------------------------------------
constexpr int PIN_STATUS_LED   = 2;   // on-board LED on most dev-kits

// --- Wi-Fi access point ------------------------------------------------------
constexpr const char* WIFI_AP_SSID = "K6-Exhibit";
constexpr const char* WIFI_AP_PASS = "phonebox";    // min 8 chars for WPA2

// --- Firmware version --------------------------------------------------------
#define FIRMWARE_VERSION "1.2.0"

// --- Timing constants -------------------------------------------------------

// Hook detection
constexpr unsigned long HOOK_DEBOUNCE_MS      = 80;
constexpr int           LINE_THRESHOLD_ON     = 800;   // ADC value: phone off-hook
constexpr int           LINE_THRESHOLD_OFF    = 300;   // ADC value: phone on-hook

// Rotary dial pulse decoding
constexpr unsigned long PULSE_MIN_BREAK_MS    = 20;
constexpr unsigned long PULSE_MAX_BREAK_MS    = 120;
constexpr unsigned long INTER_DIGIT_TIMEOUT_MS = 300;

// UK ring cadence: 400 ms ON, 200 ms OFF, 400 ms ON, 2000 ms OFF (3 s cycle)
constexpr unsigned long RING_ON_1_MS          = 400;
constexpr unsigned long RING_OFF_1_MS         = 200;
constexpr unsigned long RING_ON_2_MS          = 400;
constexpr unsigned long RING_OFF_2_MS         = 2000;
constexpr int           RING_FREQ_HZ          = 25;

// Random auto-ring interval (ms).  The phone will ring automatically at a
// random interval between these two bounds.
constexpr unsigned long AUTO_RING_MIN_MS      = 300000;   // 5 minutes
constexpr unsigned long AUTO_RING_MAX_MS      = 1800000;  // 30 minutes

// Dialling
constexpr int           MAX_DIALLED_DIGITS    = 11;
constexpr unsigned long NUMBER_COMPLETE_MS    = 3000;     // gap after last digit
constexpr unsigned long DIAL_TONE_TIMEOUT_MS  = 15000;    // idle off-hook timeout

// Button debounce
constexpr unsigned long BTN_DEBOUNCE_MS       = 50;

// A+B coin box
constexpr unsigned long COIN_DEBOUNCE_MS      = 100;   // debounce for coin/button inputs
constexpr unsigned long COIN_DETECT_BOOT_MS   = 2000;  // time at boot to detect daughter board
constexpr unsigned long COIN_BTN_A_TIMEOUT_MS = 30000; // max wait for Button A after answer

// --- SD card directory layout -----------------------------------------------
// /system/dialtone.mp3       continuous dial tone
// /system/busy.mp3           busy / error tone
// /system/not_recognised.mp3 "the number you have dialled…"
// /history/001.mp3 …         exhibit history tracks (picked at random)
// /numbers/<number>.mp3      mapped tracks keyed by dialled number
constexpr const char* SD_DIR_SYSTEM    = "/system";
constexpr const char* SD_DIR_HISTORY   = "/history";
constexpr const char* SD_DIR_NUMBERS   = "/numbers";
constexpr const char* SD_FILE_DIALTONE = "/system/dialtone.mp3";
constexpr const char* SD_FILE_BUSY     = "/system/busy.mp3";
constexpr const char* SD_FILE_NOT_REC  = "/system/not_recognised.mp3";
constexpr const char* SD_FILE_INSERT   = "/system/insert_coins.mp3"; // optional A+B prompt
constexpr const char* SD_FILE_PRESS_A  = "/system/press_a.mp3";     // optional A+B prompt
constexpr const char* SD_FILE_PRESS_B  = "/system/press_b.mp3";     // optional A+B prompt
constexpr const char* SD_FILE_REPLACE  = "/system/replace_handset.mp3"; // end-of-call reminder
constexpr const char* SD_FILE_RING_TONE = "/system/ringing_tone.mp3";   // UK ringing tone for outgoing calls

// Default outgoing call ringing tone duration (ms) before "connecting".
constexpr unsigned long DEFAULT_RING_TONE_MS = 6000;  // 6 seconds = ~2 ring cycles

// Default max ring cadences for incoming auto-ring (0 = unlimited).
constexpr int DEFAULT_MAX_RING_CADENCES = 10;
