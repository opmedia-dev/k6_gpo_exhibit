#pragma once

// ============================================================================
// K6 GPO Exhibit — ESP32 Interface for GPO 232/332 Telephones
// Pin assignments and system configuration
// ============================================================================

// --- Phone line interface ---------------------------------------------------
// Sense resistor in series with the DC line supply. The voltage across this
// resistor is divided down and fed to the ADC. When the phone goes off-hook
// current flows (~20-30 mA) producing a readable voltage; dial pulses appear
// as brief current interruptions on the same pin.
constexpr int PIN_LINE_SENSE   = 34;  // ADC1_CH6, input-only GPIO

// --- Bell / ring generator --------------------------------------------------
// An H-bridge (e.g. L293D) driven at 25 Hz toggles a boosted DC rail across
// the bell winding via the phone's 3rd (bell) wire.
constexpr int PIN_RING_EN      = 4;   // H-bridge enable (active-high)
constexpr int PIN_RING_A       = 16;  // H-bridge input A
constexpr int PIN_RING_B       = 17;  // H-bridge input B

// --- Audio I/O --------------------------------------------------------------
// First revision uses the ESP32 built-in DAC for earpiece output and ADC for
// microphone input. Coupling transformers on the board isolate these from the
// phone line DC bias.
constexpr int PIN_AUDIO_OUT    = 25;  // DAC1 — earpiece audio
constexpr int PIN_AUDIO_IN     = 36;  // ADC1_CH0 (VP) — microphone audio

// --- Status LED -------------------------------------------------------------
constexpr int PIN_STATUS_LED   = 2;   // on-board LED on most dev-kits

// --- Timing constants -------------------------------------------------------
// All times in milliseconds unless stated otherwise.

// Hook detection
constexpr unsigned long HOOK_DEBOUNCE_MS      = 80;
constexpr int           LINE_THRESHOLD_ON     = 800;   // ADC value: phone off-hook
constexpr int           LINE_THRESHOLD_OFF    = 300;   // ADC value: phone on-hook

// Rotary dial pulse decoding
constexpr unsigned long PULSE_MIN_BREAK_MS    = 20;    // ignore glitches shorter than this
constexpr unsigned long PULSE_MAX_BREAK_MS    = 120;   // break longer than this is not a pulse
constexpr unsigned long INTER_DIGIT_TIMEOUT_MS = 300;  // gap after last pulse → digit complete

// UK ring cadence: 400 ms ON, 200 ms OFF, 400 ms ON, 2000 ms OFF (3 s cycle)
constexpr unsigned long RING_ON_1_MS          = 400;
constexpr unsigned long RING_OFF_1_MS         = 200;
constexpr unsigned long RING_ON_2_MS          = 400;
constexpr unsigned long RING_OFF_2_MS         = 2000;
constexpr int           RING_FREQ_HZ          = 25;    // bell drive frequency

// Audio sample rate (built-in DAC/ADC path)
constexpr int           AUDIO_SAMPLE_RATE     = 8000;  // 8 kHz telephone quality
