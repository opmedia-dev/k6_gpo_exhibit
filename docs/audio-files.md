# Audio Files Required

This document lists all audio files the firmware expects on the SD card. Files are organised into directories by purpose.

## SD Card Directory Structure

```
/
├── system/            System sounds and configuration
├── history/           Exhibit history tracks (played at random)
├── numbers/           Audio keyed by dialled number
├── plugins/           JSON dial sequence scripts (not audio)
└── logs/              Auto-generated logs and stats (do not edit)
```

---

## /system/ — System Sounds

These are the core audio files the firmware looks for by exact path.

| File | Required | Description |
|------|----------|-------------|
| `dialtone.mp3` | **Yes** | Continuous dial tone played when the handset is lifted. Should loop cleanly. |
| `busy.mp3` | **Yes** | Busy/error tone played when a call cannot be connected. Should loop cleanly. |
| `not_recognised.mp3` | **Yes** | "The number you have dialled has not been recognised" announcement. Plays when a dialled number has no matching file or alias. |
| `ringing_tone.mp3` | **Yes** | UK ringing tone (ring-ring ... ring-ring). Played through the earpiece while waiting for a connection. |
| `replace_handset.mp3` | Recommended | End-of-call reminder. Played after audio finishes to prompt the visitor to hang up. |
| `insert_coins.mp3` | Optional | A+B coin box only. "Insert coins now" prompt played when the coin box is active. |
| `press_a.mp3` | Optional | A+B coin box only. "Press Button A to connect" instruction. |
| `press_b.mp3` | Optional | A+B coin box only. "Press Button B for your money back" instruction. |

### /system/ — Configuration Files (auto-managed)

These JSON files are created and updated automatically by the firmware via the web UI. Do not edit manually.

| File | Purpose |
|------|---------|
| `settings.json` | All device settings (volume, bell, ring timing, idle alert, etc.) |
| `aliases.json` | Number-to-filename mappings configured in the Number Directory |

---

## /history/ — Exhibit History Tracks

MP3 files in this directory are picked **at random** when a visitor answers an auto-ring call (or when no specific number is dialled). These are the main exhibit content — the stories, recordings, and historical audio that visitors hear.

**Naming:** Any `.mp3` filename works. The firmware scans the directory and picks randomly. Suggested convention: `001.mp3`, `002.mp3`, etc., or descriptive names like `history_of_k6.mp3`.

**Examples:**
```
/history/001_history_of_k6_phone_box.mp3
/history/002_local_area_1950s.mp3
/history/003_telephone_exchange_story.mp3
/history/004_post_office_memories.mp3
```

**Requirements:**
- At least one `.mp3` file is needed for the exhibit to play content
- Files should be standard MP3 format (CBR or VBR, any bitrate)
- Recommended: 128kbps mono or stereo for good quality vs. file size balance

---

## /numbers/ — Dialled Number Audio

When a visitor dials a number, the firmware looks for a matching file in this directory. The filename is determined by the Number Directory (aliases) configured in the web UI.

**How it works:**
1. Visitor dials `999`
2. Firmware checks aliases — finds `999` → `emergency`
3. Firmware plays `/numbers/emergency.mp3`

If no alias exists, the firmware tries `/numbers/999.mp3` directly (literal number as filename).

**Examples:**
```
/numbers/emergency.mp3        (mapped from 999)
/numbers/operator.mp3         (mapped from 100)
/numbers/speaking_clock.mp3   (mapped from 123)
/numbers/weather.mp3          (mapped from 200)
```

**Requirements:**
- Standard MP3 format
- Filename must match the alias name (or the literal dialled number if no alias)
- No limit on the number of files

---

## /plugins/ — Dial Sequence Scripts

JSON files that define custom multi-step sequences triggered by dialling specific numbers. These are **not audio files** but they reference audio files.

**Naming:** `<number>.json` — e.g., `999.json` is triggered when `999` is dialled.

**Example** (`/plugins/999.json`):
```json
{
  "steps": [
    {"action": "play", "file": "/system/siren.mp3"},
    {"action": "delay", "ms": 2000},
    {"action": "play", "file": "/numbers/emergency.mp3"}
  ]
}
```

**Available actions:**

| Action | Parameters | Description |
|--------|-----------|-------------|
| `play` | `file` (path) | Play an MP3 file, advance when finished |
| `delay` | `ms` (milliseconds) | Wait for the specified time |
| `loop` | `file` (path), `ms` (milliseconds) | Play file on loop for the specified time |

**Notes:**
- Maximum 10 steps per plugin
- Plugins are checked **before** normal number/alias lookup (they take priority)
- Any audio files referenced in plugin steps must exist at the specified path

---

## /logs/ — Auto-generated (Do Not Edit)

These files are created and managed automatically by the firmware.

| File | Purpose |
|------|---------|
| `system.log` | Boot events, errors, Wi-Fi, SD card, mode changes, OTA updates |
| `calls.log` | Incoming/outgoing call records with timestamps |
| `stats.json` | Visitor statistics (ring count, answer count, call durations, etc.) |
| `boot_count.txt` | Boot counter for log rotation |

---

## Audio Format Guidelines

- **Format:** MP3 (`.mp3`)
- **Bitrate:** 128kbps recommended (64–320kbps supported)
- **Sample rate:** 44.1kHz or 22.05kHz
- **Channels:** Mono or stereo (mono recommended — the telephone earpiece is mono)
- **Quality note:** The audio plays through a vintage telephone earpiece with limited frequency response (~300Hz–3.4kHz), so high-fidelity recordings are not necessary. Clear speech at 128kbps mono is ideal.

## Quick Setup Checklist

To get a working exhibit, you need at minimum:

1. Create the directories: `/system/`, `/history/`, `/numbers/`, `/plugins/`, `/logs/`
2. Add the four required system files to `/system/`:
   - `dialtone.mp3`
   - `busy.mp3`
   - `not_recognised.mp3`
   - `ringing_tone.mp3`
3. Add at least one history track to `/history/`
4. Optionally add number-specific audio to `/numbers/`
5. Optionally add `replace_handset.mp3` to `/system/`
6. Insert the SD card and power on — the firmware creates `/logs/` and config files automatically
