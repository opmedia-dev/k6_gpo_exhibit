# Bill of Materials

## Power Supply

| Qty | Component | Example Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 1 | 12 V DC adapter | Any 12 V / 1 A barrel-jack wall-wart | Main system power | £5 |
| 1 | Buck converter IC | LM2596-5.0 (TO-220-5) | 12 V → 5 V fixed-output regulator | £1.50 |
| 1 | Boost converter module | XL6009 adjustable | 12 V → 50 V for bell | £1-2 |

> **Why 12 V?** A single 5 V → 50 V boost module is hard to source (10:1 ratio).
> Starting from 12 V the XL6009 only needs a 4:1 boost — well within spec and
> widely available on Amazon/eBay/AliExpress.

## Controller & Audio

| Qty | Component | Value / Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 1 | ESP32 DevKit V1 | ESP-WROOM-32 | Main controller | £6 |
| 1 | MAX98357A I2S DAC module | — | Audio output to phone earpiece | £2 |
| 1 | Micro-SD card module | SPI breakout | MP3 file storage | £1 |
| 1 | Micro-SD card | ≥ 1 GB, FAT32 | Stores audio files | £3 |

## Phone Line Interface

| Qty | Component | Value / Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 1 | Optocoupler | PC817 (or similar) | Hook/dial isolation + detection | £0.30 |
| 1 | H-bridge motor driver | L293D (DIP-16) | Bell ring generator | £1.50 |
| 1 | Audio transformer | 600 Ω : 600 Ω 1:1 | Phone line audio coupling | £2 |

## Passive Components

| Qty | Component | Value | Label | Purpose |
|-----|-----------|-------|-------|---------|
| 1 | Resistor | 470 Ω, 1 W | R1 | Line current limit |
| 1 | Resistor | 220 Ω, ¼ W | R2 | Optocoupler LED current limit |
| 1 | Resistor | 10 kΩ, ¼ W | R3 | Pull-down for optocoupler output |
| 1 | Electrolytic capacitor | 680 µF, 25 V | C2 | LM2596 input filter |
| 1 | Electrolytic capacitor | 220 µF, 25 V | C3 | LM2596 output filter |
| 1 | Power inductor | 33 µH, ≥ 1 A saturation | L1 | LM2596 energy storage |
| 1 | Schottky diode | 1N5825 (DO-201, 5 A / 40 V) | D1 | LM2596 freewheeling diode |
| 1 | Capacitor (ceramic) | 100 nF | C1 | Decoupling for L293D |

## Control Panel

| Qty | Component | Notes |
|-----|-----------|-------|
| 3 | Momentary push buttons | RING / CANCEL / RESET |

## Connectors & Mechanical

| Qty | Component | Notes |
|-----|-----------|-------|
| 1 | 3-way screw terminal block | Phone cord connection (Line A, Line B, Bell) |
| 1 | Barrel jack socket | 12 V power input |
| 1 | Project box / enclosure | Houses all electronics + buttons |
| 1 | Multi-core cable (≥ 6 conductors) | Extended lead from phone to enclosure |
| 1 | Custom carrier PCB | 100 × 100 mm, 2-layer (see `pcb/` folder for Gerbers) |
| — | Hook-up wire, solder, standoffs | Assembly |

## Optional

| Qty | Component | Purpose |
|-----|-----------|---------|
| 1 | Electromagnetic bell (Bellset No. 26) | External bell for GPO 232 |
| 1 | USB Micro-B cable | For reprogramming / serial debug |
| 1 | 3D-printed enclosure | Tidier than a generic project box |

## Total Estimated Cost

| Category | Cost |
|----------|------|
| Power supply (adapter + buck IC/passives + boost module) | ~£9 |
| Controller + audio (ESP32 + DAC + SD) | ~£12 |
| Phone line interface (opto + H-bridge + transformer) | ~£4 |
| Passives + connectors | ~£4 |
| **Total** | **~£29** |

*Excludes: phone, SD card content, enclosure, extended cable.*

## Where to Buy (UK)

- **Amazon UK** — ESP32 DevKit, XL6009 modules, SD card modules
- **RS Components / Mouser** — LM2596-5.0, 1N5825, 33 µH inductor, electrolytic caps
- **eBay** — PC817 optocouplers, L293D, 600 Ω audio transformers, push buttons
- **CPC/Farnell** — Resistors, capacitors, screw terminals, stripboard
- **AliExpress** — Cheapest for modules (longer delivery)
- **Pimoroni / The Pi Hut** — ESP32 boards, breakout modules
