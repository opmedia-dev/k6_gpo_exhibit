# K6 GPO Exhibit

ESP32-based interface board that lets you use an **unmodified** GPO 232 or 332
rotary-dial telephone over its original 3-core cord.  Designed for a K6
telephone box exhibit, but works with any standalone GPO phone.

## What It Does

The ESP32 simulates a telephone exchange line card.  Connect the phone's
existing cord to the interface board and the phone works exactly as it did in
service:

- **Lift the handset** → hear a UK dial tone
- **Rotary dial a number** → digits decoded and printed to serial (+ callbacks)
- **Ring the bell** → triggered via serial command or application code
- **Bidirectional audio** → microphone and earpiece work through the coupling
  transformer

No modifications are made to the telephone.

## Supported Phones

| Phone | Bell | Notes |
|-------|------|-------|
| GPO 232 | External (Bellset No. 26) | Blue wire drives external bell, or leave unconnected |
| GPO 332 | Internal (2 µF cap + bell coils) | Full bell ringing via blue wire |

## Hardware

See [`docs/hardware/`](docs/hardware/) for:
- [**Schematic**](docs/hardware/schematic.md) — full circuit with ASCII diagrams
- [**Bill of Materials**](docs/hardware/bom.md) — ~£14 in components
- [**Wiring Guide**](docs/hardware/wiring-guide.md) — step-by-step build + test

### Quick Summary

```
GPO Phone ←→ 3-core cord ←→ [Terminal Block] ←→ ESP32 Interface Board
                                                    │
                                              USB power + serial
```

Key sub-circuits:
1. **Hook / dial detection** — optocoupler senses line current through a
   series resistor
2. **Ring generator** — boost converter (50 V) + L293D H-bridge at 25 Hz
3. **Audio coupling** — 600 Ω transformer isolates ESP32 DAC/ADC from line DC

## Firmware

Built with [PlatformIO](https://platformio.org/) (Arduino framework for
ESP32).

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
| `H` | Hang up / stop ringing |
| `S` | Print current state |

### Architecture

```
main.cpp                  Arduino setup/loop, serial commands
  └─ PhoneController      Top-level state machine
       ├─ PhoneLine       Hook detection, line current ADC
       ├─ RotaryDecoder   Dial pulse counting → digit
       ├─ BellDriver      25 Hz H-bridge cadence driver
       └─ AudioInterface  DAC/ADC telephone audio
```

States: `IDLE → RINGING → CONNECTED` (incoming) or
`IDLE → DIAL_TONE → DIALING → CONNECTED` (outgoing).

### Callbacks

Register handlers in your application code:

```cpp
phone.onDigit([](uint8_t d)       { /* each digit as dialled */ });
phone.onNumber([](const char* n)  { /* complete number string */ });
phone.onHook([](HookState s)      { /* hook state change */     });
```

## Pin Assignments

| GPIO | Function |
|------|----------|
| 34 | Line sense (hook/dial) — ADC input |
| 36 | Audio in (microphone) — ADC input |
| 25 | Audio out (earpiece) — DAC output |
| 4 | Ring enable (H-bridge EN) |
| 16 | Ring phase A (H-bridge IN1) |
| 17 | Ring phase B (H-bridge IN2) |
| 2 | Status LED |

## Technical Reference

See [`docs/gpo-phone-reference.md`](docs/gpo-phone-reference.md) for detailed
GPO 232/332 electrical specifications, rotary dial pulse characteristics, and
bell ringing requirements.

## Future Enhancements

- **I2S audio codec** (MAX98357A + INMP441) for better audio quality
- **Wi-Fi / MQTT** integration for smart-home or exhibit control
- **MP3 playback** — play recorded messages or exhibit narration
- **Multi-phone networking** — connect two GPO phones via ESP-NOW
- **VoIP gateway** — bridge the GPO phone to SIP/VoIP
- **KiCad PCB** — proper PCB layout (currently prototyped on stripboard)

## License

MIT
