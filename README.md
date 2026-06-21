# K6 GPO Exhibit

ESP32-based interface that lets an **unmodified** GPO 232 or 332 rotary-dial
telephone work as an interactive exhibit inside a K6 phone box.

Visitors can pick up the handset, hear a dial tone, dial any number,
and listen to curated audio content from an SD card.  The phone rings
at random intervals; answering plays a randomly selected history track.
An optional A+B coin box daughter board adds pre-payment coin mechanism
support.

---

## Features

- **Random ringing** -- the phone rings at configurable intervals
  (default 5-30 min); answering plays a randomly selected history track
- **Dial-a-number** -- dial up to 11 digits; a matching MP3 plays from
  the SD card, or a "number not recognised" message plays
- **External control panel** -- four buttons (RING / CANCEL / RESET / MODE)
  with a panel lamp indicating auto-ring mode
- **Full GPO phone interface** -- hook detection, rotary pulse decoding,
  bell ringing (25 Hz UK cadence), all through the original 3-core cord
- **SD card audio** -- all audio loaded from a micro-SD card as MP3 files
- **I2S audio output** -- MAX98357A DAC with coupling transformer feeds
  the phone's earpiece
- **Auto / Manual mode** -- MODE button toggles auto-ring on/off; panel
  lamp shows current state
- **Wi-Fi file manager** -- ESP32 creates a `K6-Exhibit` hotspot; connect
  from any phone or laptop browser to upload, delete, and manage SD card
  files, adjust settings, view visitor stats, and flash firmware
- **OTA firmware update** -- upload a compiled `.bin` through the web UI
- **Discovery logging** -- unrecognised dialled numbers are logged so
  staff can see what content visitors expect; viewable via the web UI
- **Optional A+B coin box** -- auto-detected daughter board supports
  classic GPO pre-payment coin mechanisms (Button A/B, coin weight switch)

No modifications are made to the telephone.

---

## Supported Phones

| Phone | Bell | Notes |
|-------|------|-------|
| GPO 232 | External (Bellset No. 26) | Blue wire drives external bell, or leave unconnected |
| GPO 332 | Internal (2 uF cap + bell coils) | Full bell ringing via blue wire |
| GPO 332L | Internal | "Listener" variant, same 3-wire connection |

Any GPO phone with a 3-wire connection (Line A, Line B, Bell) should work.
See [`docs/gpo-phone-reference.md`](docs/gpo-phone-reference.md) for
detailed electrical specs.

---

## System Overview

```
+---------------+   3-core cord    +-------------------------------+
|  GPO Phone    | <--------------> |   Carrier Board               |
|  (unmodified) |  extended lead   |   (in separate enclosure)     |
+---------------+                  |                               |
                                   |  ESP32 DevKit V1 (30-pin)     |
                                   |  + Micro-SD card module       |
                                   |  + MAX98357A I2S DAC          |
                                   |  + L293D H-bridge (ringer)    |
                                   |  + PC817 optocoupler (hook)   |
                                   |  + 12V DC -> LM2596 buck (5V) |
                                   |  + 48V DC (bell supply)       |
                                   |  + 5x 1N4007 protection diodes|
                                   |  + 4 control buttons + lamp   |
                                   +-------------------------------+
                                             |
                                   +---------+---------+
                                   | Daughter Board     |  (optional)
                                   | 3x optocoupler     |
                                   | A+B coin box I/O   |
                                   +-------------------+
```

### Power

Two separate DC adapters power the system:

| Supply | Voltage | Purpose |
|--------|---------|---------|
| 12V DC (1A) | Logic | LM2596 buck converter produces 5V for ESP32, DAC, SD card, L293D logic |
| 48V DC (1A) | Bell | L293D H-bridge pin 8 (VS) for bell ringing |

Separate supplies eliminate voltage dips during bell ringing that
previously caused ESP32 resets.

### Protection

Five 1N4007 rectifier diodes protect the circuit:

| Diode | Purpose |
|-------|---------|
| D1 | Anti-parallel across PC817 LED -- clamps 48V reverse voltage during ringing to 0.7V |
| D2-D5 | Overvoltage clamps on transformer primary (DAC_LP and DAC_LN to +5V and GND) |

---

## Hardware

Full hardware documentation is in [`docs/hardware/`](docs/hardware/):

- [**Schematic**](docs/hardware/schematic.md) -- complete circuit with
  ASCII diagrams for every sub-circuit
- [**Bill of Materials**](docs/hardware/bom.md) -- full parts list with
  approximate costs (~£42 for carrier board, ~£5 for optional daughter board)
- [**Wiring Guide**](docs/hardware/wiring-guide.md) -- step-by-step build
  and testing instructions

### PCB

KiCad project files for both boards are in [`pcb/`](pcb/):

| Board | Size | Description |
|-------|------|-------------|
| [Carrier board](pcb/README.md) | 100 x 100 mm | Main board, 2-layer, all through-hole |
| [Daughter board](pcb/COINBOX_DAUGHTER.md) | 45 x 35 mm | Optional A+B coin box interface |

Pre-generated Gerber files are in `pcb/gerbers/` -- ready to upload to
JLCPCB, PCBWay, or any other PCB fabricator.

---

## SD Card Setup

Format a micro-SD card as FAT32 and create this directory structure:

```
/system/
    dialtone.mp3           Continuous dial tone (loops while off-hook)
    busy.mp3               Busy / error tone (loops)
    not_recognised.mp3     "The number you have dialled..."
    ringing_tone.mp3       UK ringing tone (ring-ring, pause)
/history/
    001.mp3                History track 1 (played at random on ring answer)
    002.mp3                History track 2
    ...
/numbers/
    999.mp3                Plays when visitor dials 999
    100.mp3                Plays when visitor dials 100
    08001111.mp3           Plays when visitor dials 08001111
    ...
```

See [`docs/audio-files.md`](docs/audio-files.md) for the complete list
of required and optional audio files, plugin scripts, and format guidelines.

---

## Wi-Fi File Manager

The ESP32 creates a Wi-Fi access point on boot:

| Setting | Value |
|---------|-------|
| SSID | `K6-Exhibit` |
| Password | `phonebox` |
| URL | `http://192.168.4.1/` |

Connect with any phone or laptop, open a browser, and you can:

- Adjust volume, ring timing, and mode
- View visitor statistics and call history
- Browse, upload, and delete SD card files
- View discovery log (numbers visitors tried that have no content)
- Flash new firmware over the air (OTA)
- View system status (heap, SD card space, current state)

---

## Firmware

Built with [PlatformIO](https://platformio.org/) (Arduino framework for ESP32).

### Build and Flash

```bash
cd firmware
pio run                    # compile
pio run -t upload          # flash to ESP32
pio device monitor         # serial console (115200 baud)
```

See [`docs/firmware-flashing-guide.md`](docs/firmware-flashing-guide.md)
for full first-time setup instructions (Python, PlatformIO, USB drivers,
troubleshooting) for Windows, macOS, and Linux.

### Serial Commands

| Command | Action |
|---------|--------|
| `R` | Ring the bell |
| `H` | Hang up / stop everything |
| `C` | Cancel ring / stop playback |
| `S` | Print current state and diagnostics |
| `A` | Toggle auto-ring on/off |
| `V0`-`V9` | Set volume (0 = min, 9 = max) |

### Control Panel

| Button | GPIO | Function |
|--------|------|----------|
| RING | 32 | Manually trigger the phone bell |
| CANCEL | 33 | Cancel ringing / stop current playback |
| RESET | 27 | Full system restart |
| MODE | 14 | Toggle auto/manual ring mode |

All buttons are active-low (connect to GND when pressed); the ESP32's
internal pull-ups are enabled in firmware.

**Panel lamp** (GPIO 13) -- illuminates when auto-ring mode is active.

### Firmware Architecture

```
main.cpp                    Arduino setup/loop, serial commands
  +-- PhoneController        Top-level state machine
       +-- PhoneLine         Hook detection via optocoupler + ADC
       +-- RotaryDecoder     Dial pulse counting -> digit
       +-- BellDriver        25 Hz H-bridge with UK ring cadence
       +-- AudioPlayer       SD card MP3 playback via I2S
       +-- ControlPanel      4-button debounced input
       +-- CoinBox           Optional A+B coin box (auto-detected)
       +-- StatsTracker      Visitor stats + discovery logging
       +-- WebManager        Wi-Fi AP, file manager, OTA, REST API
```

### State Machine

```
             auto-ring timer
                  or
IDLE ---- BTN_RING/serial 'R' ---- RINGING ---- (answer) ---- PLAYING_HISTORY
  |                                                                 |
  |                                                            (on-hook)
  |                                                                 |
  +-- (lift handset) -- DIAL_TONE -- DIALING -+-- PLAYING_NUMBER ---+
                                              |         |
                                              |    (playback ends) -- BUSY -- (on-hook) -- IDLE
                                              |
                             [A+B, recognised] +-- AWAIT_COINS -- (coins in) -- AWAIT_BTN_A
                                              |                   "press A" prompt
                             [A+B, coins in]  +-- AWAIT_BTN_A -- (Btn A) -- PLAYING_NUMBER
                                              |                  (Btn B) -- IDLE (refund)
                                              |
                         [A+B, not recognised] +-- AWAIT_BTN_B -- (Btn B) -- IDLE (refund)
                                                   "not recognised" then "press B" prompt

Without A+B: DIALING -> PLAYING_NUMBER or PLAYING_NOT_REC directly.
Incoming calls (RINGING -> answer) never require A+B interaction.
```

---

## Pin Assignments

| GPIO | Function |
|------|----------|
| 34 | Line sense (hook / dial pulse) -- ADC input |
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
| 13 | Auto-mode lamp (active-high) |
| 2 | Status LED |
| 36 | Coin box: coin sense (optional, input-only) |
| 39 | Coin box: Button A (optional, input-only) |
| 35 | Coin box: Button B (optional, input-only) |

See [`pcb/PINOUT.md`](pcb/PINOUT.md) for the complete carrier board
pinout with every connector, IC, and passive component pin-mapped.

---

## Repository Structure

```
k6_gpo_exhibit/
+-- firmware/                 PlatformIO project (ESP32 Arduino)
|   +-- src/                  Source code
|   +-- platformio.ini        Build configuration
+-- pcb/                      KiCad PCB design files
|   +-- k6_carrier_rev2.*     Carrier board (KiCad project, schematic, PCB)
|   +-- k6_coinbox_daughter.* Daughter board (KiCad project, schematic, PCB)
|   +-- gerbers/              Pre-generated Gerber + drill files
|   +-- generate_pcb.py       Carrier board generator script
|   +-- generate_coinbox_daughter.py  Daughter board generator script
|   +-- PINOUT.md             Complete carrier board pinout reference
|   +-- COINBOX_DAUGHTER.md   Daughter board circuit and wiring guide
|   +-- README.md             PCB fabrication instructions
+-- docs/
|   +-- hardware/
|   |   +-- schematic.md      Full circuit diagrams
|   |   +-- bom.md            Bill of materials with costs
|   |   +-- wiring-guide.md   Step-by-step build and test instructions
|   +-- audio-files.md        SD card audio file reference
|   +-- firmware-flashing-guide.md  First-time flashing (Windows/macOS/Linux)
|   +-- gpo-phone-reference.md     GPO 232/332 electrical specifications
+-- preview.html              Web UI preview (standalone HTML)
```

---

## Technical Reference

See [`docs/gpo-phone-reference.md`](docs/gpo-phone-reference.md) for
GPO 232/332 electrical specifications, rotary dial pulse characteristics,
bell ringing requirements, and 3-core cord wiring.

---

## License

MIT
