# K6 GPO Exhibit

ESP32-based interface that lets an **unmodified** GPO 232 or 332 rotary-dial
telephone work as an interactive exhibit in a K6 phone box.

## Features

- **Random ringing** — the phone rings at configurable random intervals
  (default 5-30 min); answering plays a randomly selected history track
- **Dial-a-number** — dial up to 11 digits; if a matching MP3 exists on the
  SD card it plays, otherwise a "number not recognised" message plays
- **External control box** — three buttons (RING / CANCEL / RESET) on the
  operator's equipment box, connected via an extended lead
- **Full GPO phone interface** — hook detection, rotary pulse decoding, bell
  ringing (25 Hz UK cadence), all through the original 3-core cord
- **SD card audio** — all audio (history tracks, number tracks, dial tone,
  busy tone) loaded from a micro-SD card as MP3 files
- **I2S audio output** — MAX98357A DAC for quality playback through the
  phone's earpiece via a coupling transformer
- **Optional A+B coin box** — auto-detected daughter board supports classic
  GPO pre-payment coin mechanisms (Button A/B, coin weight switch)

No modifications are made to the telephone.

## Supported Phones

| Phone | Bell | Notes |
|-------|------|-------|
| GPO 232 | External (Bellset No. 26) | Blue wire drives external bell, or leave unconnected |
| GPO 332 | Internal (2 µF cap + bell coils) | Full bell ringing via blue wire |
| GPO 332L | Internal | "Listener" variant, same 3-wire connection |

Any GPO phone with a 3-wire connection (Line A, Line B, Bell) should work.
See the hardware docs for notes on other models (706, 746, etc.).

## System Architecture

```
┌─────────────┐   3-core cord    ┌─────────────────────────────┐
│  GPO Phone  │ ◄──────────────► │   ESP32 Interface Board     │
│  (unmod.)   │  extended lead   │   (in separate enclosure)   │
└─────────────┘                  │                             │
                                 │  ESP32 DevKit               │
                                 │  + SD card module           │
                                 │  + MAX98357A I2S DAC        │
                                 │  + L293D H-bridge (ringer)  │
                                 │  + 12V adapter → buck (5V)  │
                                 │    + XL6009 boost (50V)     │
                                 │  + Optocoupler (line sense) │
                                 │  + 3 control buttons        │
                                 └─────────────────────────────┘
```

## Hardware

See [`docs/hardware/`](docs/hardware/) for:
- [**Schematic**](docs/hardware/schematic.md) — full circuit with ASCII diagrams
- [**Bill of Materials**](docs/hardware/bom.md) — ~£29 in components
- [**Wiring Guide**](docs/hardware/wiring-guide.md) — step-by-step build + test

### PCB

A KiCad carrier board layout is in [`pcb/`](pcb/) — 100 × 80 mm, 2-layer,
all through-hole.  All modules (ESP32 DevKit, MAX98357A, SD card, buck/boost
converters) plug in via pin headers.  See [`pcb/README.md`](pcb/README.md)
for fabrication instructions.

## SD Card Setup

Format a micro-SD card as FAT32 and create this directory structure:

```
/system/
    dialtone.mp3           Continuous dial tone (loops while off-hook)
    busy.mp3               Busy / error tone (loops)
    not_recognised.mp3     "The number you have dialled…"
/history/
    001.mp3                History track 1 (picked at random on ring answer)
    002.mp3                History track 2
    …
/numbers/
    999.mp3                Plays when user dials 999
    100.mp3                Plays when user dials 100
    08001111.mp3           Plays when user dials 08001111
    …
```

- History tracks play when the phone rings and is answered
- Number tracks are matched by filename: dialling `999` looks for `/numbers/999.mp3`
- If no match is found, `/system/not_recognised.mp3` plays

## Firmware

Built with [PlatformIO](https://platformio.org/) (Arduino framework for ESP32).

### Build & Flash

```bash
cd firmware
pio run                    # compile
pio run -t upload          # flash to ESP32
pio device monitor         # serial console (115200 baud)
```

### Serial Commands

| Command | Action |
|---------|--------|
| `R` | Ring the bell |
| `H` | Hang up / stop everything |
| `C` | Cancel ring / stop playback |
| `S` | Print current state + diagnostics |
| `A` | Toggle auto-ring on/off |
| `V0`-`V9` | Set volume (0 = min, 9 = max) |

### Control Panel Buttons

| Button | Function |
|--------|----------|
| RING (GPIO 32) | Manually trigger the phone bell |
| CANCEL (GPIO 33) | Cancel ringing / stop current playback |
| RESET (GPIO 27) | Full system restart |
| MODE (GPIO 14) | Toggle auto/manual ring mode |

All buttons are active-low (connect to GND when pressed); the ESP32's
internal pull-ups are enabled.

### Auto-Mode Indicator Lamp

GPIO 13 drives a 3–6 V panel lamp (active-high) that illuminates when
auto-ring mode is active.  In **auto mode** the phone rings at random
intervals (5–30 min) and the lamp is ON.  In **manual mode** the phone
only rings when the RING button is pressed and the lamp is OFF.
Calling (lifting the handset, dialling) works in both modes.

### Firmware Architecture

```
main.cpp                    Arduino setup/loop, serial commands
  └─ PhoneController        Top-level state machine
       ├─ PhoneLine         Hook detection via optocoupler + ADC
       ├─ RotaryDecoder     Dial pulse counting → digit
       ├─ BellDriver        25 Hz H-bridge with UK ring cadence
       ├─ AudioPlayer       SD card MP3 playback via I2S
       ├─ ControlPanel      3-button debounced input
       └─ CoinBox           Optional A+B coin box (auto-detected)
```

State machine:

```
             auto-ring timer
                  or
IDLE ──── BTN_RING/serial 'R' ──── RINGING ──── (answer) ──── PLAYING_HISTORY
  │                                                                 │
  │                                                            (on-hook)
  │                                                                 │
  └── (lift handset) ── DIAL_TONE ── DIALING ─┬── PLAYING_NUMBER ──┘
                                               │         │
                                               │    (playback ends) ── BUSY ── (on-hook) ── IDLE
                                               │
                              [A+B, recognised] ├── AWAIT_COINS ── (coins in) ── AWAIT_BTN_A
                                               │                   "press A" prompt
                              [A+B, coins in]  ├── AWAIT_BTN_A ── (Btn A) ── PLAYING_NUMBER
                                               │                  (Btn B) ── IDLE (refund)
                                               │
                          [A+B, not recognised] └── AWAIT_BTN_B ── (Btn B) ── IDLE (refund)
                                                    "not recognised" then "press B" prompt

Without A+B: DIALING → PLAYING_NUMBER or PLAYING_NOT_REC directly.
Incoming calls (RINGING → answer) never require A+B interaction.
```

### Callbacks

```cpp
phone.onDigit([](uint8_t d)       { /* each digit as dialled */ });
phone.onNumber([](const char* n)  { /* complete number string */ });
phone.onHook([](HookState s)      { /* hook state change */     });
phone.onState([](PhoneState s)    { /* state transition */      });
```

## Pin Assignments

| GPIO | Function |
|------|----------|
| 34 | Line sense (hook / dial pulse) — ADC input |
| 26 | I2S BCLK (to MAX98357A) |
| 25 | I2S LRCLK (to MAX98357A) |
| 22 | I2S DOUT (to MAX98357A) |
| 5 | SD card CS (SPI) |
| 23 | SD card MOSI |
| 19 | SD card MISO |
| 18 | SD card SCK |
| 4 | Ring enable (H-bridge EN) |
| 16 | Ring phase A (H-bridge IN1) |
| 17 | Ring phase B (H-bridge IN2) |
| 32 | Button: RING (active-low) |
| 33 | Button: CANCEL (active-low) |
| 27 | Button: RESET (active-low) |
| 14 | Button: MODE (active-low) |
| 13 | Auto-mode lamp (active-high, 3–6 V) |
| 2 | Status LED |
| 36 | Coin box: coin sense (optional, input-only) |
| 39 | Coin box: Button A (optional, input-only) |
| 35 | Coin box: Button B (optional, input-only) |

## Technical Reference

See [`docs/gpo-phone-reference.md`](docs/gpo-phone-reference.md) for detailed
GPO 232/332 electrical specifications, rotary dial pulse characteristics, and
bell ringing requirements.

## Future Enhancements

- **Wi-Fi / MQTT** integration for remote exhibit control
- **Multi-phone networking** — connect two GPO phones via ESP-NOW
- **VoIP gateway** — bridge the GPO phone to SIP/VoIP
- **A+B coin box daughter board** — PCB design for coin mechanism interface
- **OLED status display** on the operator's control box

## License

MIT
