# K6 GPO Exhibit — Carrier Board Pinout Reference

Full pinout for every connector and IC on the carrier board. Silkscreen
labels on the PCB use short abbreviations — this document provides the
complete mapping.

> **Note — this documents the auto-generated reference layout**
> (`k6_gpo_carrier.kicad_pcb`, on-board LM2596 + boost stage). The
> **as-built manufacturing master is `k6_carrier_rev2-8.kicad_pcb`**
> (see [README.md](README.md)), which uses plug-in buck/48 V supplies and
> the **Rev 2.1 opto fix**: the PC817 LED is *in series* in the loop
> return leg via **R7 (R_LIM 2.2 kΩ) → D1 (1N4007) → LED → GND**, with
> **R2 = 470 Ω** as a shunt across the LED and **C3 (Cc, 10 µF/63 V)** as
> a DC-block coupling cap in series with the transformer secondary. Where
> this reference layout differs (opto pin 2 → LINE_B, R2 = 220 Ω, +50 V
> boost), the Rev 2.1 values below and in the schematic take precedence.

## Silkscreen Abbreviation Legend

| Abbreviation | Meaning |
|-------------|---------|
| SN | LINE_SENSE (hook detect ADC) |
| RG | BTN_RING (manual ring button) |
| CN | BTN_CANCEL (cancel button) |
| LR | I2S_LRCLK (word select) |
| BC | I2S_BCLK (bit clock) |
| RS | BTN_RESET (reset button) |
| MO | SPI_MOSI |
| MI | SPI_MISO |
| CK | SPI_SCK |
| CS | SD_CS (SD card chip select) |
| DIN | I2S_DOUT (data to DAC) |
| RB | RING_B (bell H-bridge B) |
| RA | RING_A (bell H-bridge A) |
| REN | RING_EN (bell enable) |
| LD | LED_STATUS |
| VIN | LM2596 pin 1 (12V input) |
| SW | LM2596 pin 2 (switch output) |
| FB | LM2596 pin 4 (feedback / +5V) |
| ON | LM2596 pin 5 (ON/OFF) |
| LnA | LINE_A (phone line) |
| LnB | LINE_B (phone line) |
| An | Anode |
| Kth | Cathode |
| Col | Collector |
| Em | Emitter |

---

## J3 — 12V DC Power Input

2-pin screw terminal (rotated 90°)

| Pin | Net | Description |
|-----|-----|-------------|
| 1 | +12V | DC input from wall adapter |
| 2 | GND | Ground |

## U4 — LM2596-5.0 Buck Converter IC (12V → 5V)

> **Rev 2.1:** the on-board buck (`U4` + `L1` + the buck electrolytics `C2`/`C3`
> and `D1` 1N5825) is **not placed** on the as-built master — 12 V→5 V regulation
> is done off-board. This section and the supporting-components table below are
> retained only for the legacy auto-generated layout. See the *Legacy auto-generated
> layout only* note under Passive Components.

TO-220-5 package (legacy on-board layout, with supporting passives).

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | VIN | +12V | Input voltage (12V) |
| 2 | SW | SW_OUT | Switch output → D1 cathode / L1 input |
| 3 | GND | GND | Ground (also connected to heatsink tab) |
| 4 | FB | +5V | Feedback (fixed 5V version → connects to output) |
| 5 | ON | GND | ON/OFF control (GND = always on) |

### Supporting Buck Converter Components

| Ref | Value | Net 1 | Net 2 | Description |
|-----|-------|-------|-------|-------------|
| C2 | 680 µF 25V electrolytic | +12V | GND | Input filter capacitor |
| L1 | 33 µH power inductor | SW_OUT | +5V | Energy storage inductor |
| D1 | 1N5825 (5A 40V Schottky) | GND (anode) | SW_OUT (cathode) | Freewheeling diode |
| C3 | 220 µF 25V electrolytic | +5V | GND | Output filter capacitor |

**Circuit:** +12V → CIN (C2) → U4 VIN (pin 1). U4 OUTPUT (pin 2) → L1 → +5V rail.
D1 cathode connects to pin 2/L1 junction (SW_OUT); D1 anode to GND.
COUT (C3) smooths the +5V output. Pin 5 tied to GND for always-on operation.

## J2 — 48 V Bell Supply Input

> **Rev 2 / 2.1:** the bell is powered from a **dedicated 48 V DC
> adapter**, not the Rev 1 XL6009 boost converter. On the as-built master
> J2 is a 2-pin +48V/GND input feeding L293D pin 8. The 4-pin boost
> pinout below is retained only for the legacy auto-generated layout.

4-pin header (legacy boost layout)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | 12V+ | +12V | Input positive |
| 2 | GND | GND | Input negative |
| 3 | 48V+ | +48V | Output positive (dedicated 48 V adapter in Rev 2/2.1) |
| 4 | GND | GND | Output negative |

## U1 — ESP32 DevKit V1

2×15 pin header socket (25.4 mm row spacing, 2.54 mm pin pitch)

### Left Column (top → bottom)

| Pin | Silk | GPIO | Net | Function |
|-----|------|------|-----|----------|
| L1 | 3V3 | — | +3V3 | 3.3V output |
| L2 | EN | — | — | Enable (reset) |
| L3 | 36 | 36 | — | Input only |
| L4 | 39 | 39 | — | Input only |
| L5 | 34 SN | 34 | LINE_SENSE | Hook detect ADC input |
| L6 | 35 | 35 | — | Unused |
| L7 | 32 RG | 32 | BTN_RING | Manual ring button |
| L8 | 33 CN | 33 | BTN_CANCEL | Cancel button |
| L9 | 25 LR | 25 | I2S_LRCLK | I2S word select clock |
| L10 | 26 BC | 26 | I2S_BCLK | I2S bit clock |
| L11 | 27 RS | 27 | BTN_RESET | Reset button |
| L12 | 14 | 14 | — | Unused |
| L13 | 12 | 12 | — | Unused |
| L14 | GND | — | GND | Ground |
| L15 | 13 | 13 | — | Unused |

### Right Column (top → bottom)

| Pin | Silk | GPIO | Net | Function |
|-----|------|------|-----|----------|
| R1 | 5V | — | +5V | 5V input from buck converter |
| R2 | GND | — | GND | Ground |
| R3 | 23 MO | 23 | SPI_MOSI | SD card data out |
| R4 | 22 DIN | 22 | I2S_DOUT | I2S data to DAC |
| R5 | TX | TX0 | — | Serial TX (debug) |
| R6 | RX | RX0 | — | Serial RX (debug) |
| R7 | 21 | 21 | — | Unused |
| R8 | 19 MI | 19 | SPI_MISO | SD card data in |
| R9 | 18 CK | 18 | SPI_SCK | SD card SPI clock |
| R10 | 5 CS | 5 | SD_CS | SD card chip select |
| R11 | 17 RB | 17 | RING_B | Bell H-bridge input B |
| R12 | 16 RA | 16 | RING_A | Bell H-bridge input A |
| R13 | 4 REN | 4 | RING_EN | Bell H-bridge enable |
| R14 | 2 LD | 2 | LED_STATUS | On-board LED |
| R15 | 15 | 15 | — | Unused |

## J5 — MAX98357A I2S DAC Module

7-pin header

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | 5V | +5V | Module power |
| 2 | GND | GND | Ground |
| 3 | SD | — | Shutdown (leave floating or tie high) |
| 4 | GAIN | — | Gain select (leave floating = 9 dB) |
| 5 | DIN | I2S_DOUT | I2S data input (from ESP32 GPIO 22) |
| 6 | BCK | I2S_BCLK | I2S bit clock (from ESP32 GPIO 26) |
| 7 | LRC | I2S_LRCLK | I2S word select (from ESP32 GPIO 25) |

## J6 — DAC Audio Output

2-pin header (connects to transformer T1 primary)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | SPK+ | AUDIO_P1 | Speaker output + |
| 2 | SPK- | AUDIO_P2 | Speaker output − |

## T1 — Audio Coupling Transformer

600 Ω : 600 Ω, 4-pin

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | SPK+ | AUDIO_P1 | Primary (from DAC SPK+) |
| 2 | SPK- | AUDIO_P2 | Primary (from DAC SPK−) |
| 3 | LnA | LINE_A | Secondary → phone Line A |
| 4 | LnB | LINE_B | Secondary → phone Line B |

## J4 — Phone Cord

3-pin screw terminal (rotated 90°, pitch 5.08 mm)

| Pin | Silk | Net | Wire Colour | Description |
|-----|------|-----|-------------|-------------|
| 1 | A Red | LINE_A | Red | Line A |
| 2 | B Wht | LINE_B | White | Line B |
| 3 | Bell | BELL | Blue | Bell coil connection |

## J8 — Ring Button

2-pin header (active low, internal pull-up)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | RING | BTN_RING | Button signal → ESP32 GPIO 32 |
| 2 | GND | GND | Ground |

## J9 — Cancel Button

2-pin header (active low, internal pull-up)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | CANCEL | BTN_CANCEL | Button signal → ESP32 GPIO 33 |
| 2 | GND | GND | Ground |

## J10 — Reset Button

2-pin header (active low, internal pull-up)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | RESET | BTN_RESET | Button signal → ESP32 GPIO 27 |
| 2 | GND | GND | Ground |

## J7 — Micro-SD Card Module (SPI)

6-pin header

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | GND | GND | Ground |
| 2 | 3V3 | +3V3 | Module power |
| 3 | MOSI | SPI_MOSI | Data out → ESP32 GPIO 23 |
| 4 | MISO | SPI_MISO | Data in ← ESP32 GPIO 19 |
| 5 | SCK | SPI_SCK | SPI clock → ESP32 GPIO 18 |
| 6 | CS | SD_CS | Chip select → ESP32 GPIO 5 |

## U2 — L293D H-Bridge Motor Driver (DIP-16)

| Pin | Side | Silk | Net | Description |
|-----|------|------|-----|-------------|
| 1 | L | EN | RING_EN | Enable 1/2 → ESP32 GPIO 4 |
| 2 | L | IN1 | RING_A | Input 1 → ESP32 GPIO 16 |
| 3 | L | BELL | BELL | Output 1 → bell coil |
| 4 | L | GND | GND | Ground |
| 5 | L | GND | GND | Ground |
| 6 | L | LnB | LINE_B | Output 2 → phone Line B |
| 7 | L | IN2 | RING_B | Input 2 → ESP32 GPIO 17 |
| 8 | L | 48V | +48V | Bell supply (dedicated 48 V adapter; +50V on legacy boost layout) |
| 9 | R | GND | GND | Ground |
| 10 | R | — | — | Not connected |
| 11 | R | — | — | Not connected |
| 12 | R | — | GND | Ground |
| 13 | R | — | GND | Ground |
| 14 | R | — | — | Not connected |
| 15 | R | — | — | Not connected |
| 16 | R | 5V | +5V | Logic supply |

## U3 — PC817 Optocoupler (DIP-4)

| Pin | Side | Silk | Net | Description |
|-----|------|------|-----|-------------|
| 1 | L | An | OPTO_A | LED anode — from D1 cathode (Rev 2.1); R2 470 Ω shunt to GND across the LED |
| 2 | L | Kth | GND | LED cathode → GND (Rev 2.1). *Legacy auto-layout: → LINE_B* |
| 3 | R | Em | OPTO_EMIT | Phototransistor emitter → R3 → GND, and → GPIO 34 |
| 4 | R | Col | +3V3 | Phototransistor collector → 3.3V |

**Hook detect circuit (Rev 2.1, as-built):** the opto LED is *in series*
in the loop return leg: +12V → R1 (470 Ω) → Line A → phone → Line B →
**R7 / R_LIM (2.2 kΩ)** → **D1 (1N4007, anode→Line B side)** → PC817 pin 1
(anode) → PC817 pin 2 (cathode) → GND, with **R2 (470 Ω)** across pins 1↔2.
When the phone is off-hook, loop current lights the LED; the
phototransistor output (pin 3) is read by ESP32 GPIO 34 (ADC). R7 is
**mandatory before applying 48 V** — it is the only current limit in this
leg. *(The legacy auto-generated layout wired R2 220 Ω in series and pin 2
to LINE_B, which carries no loop current and does not detect hook.)*

## Passive Components

These are the parts actually fitted on the as-built Rev 2.1 manufacturing master
(`k6_carrier_rev2-8.kicad_pcb`). Values/nets verified against that board.

| Ref | Value | Silk | Net 1 | Net 2 | Function |
|-----|-------|------|-------|-------|----------|
| R1 | 470 Ω 1W | R1 470R | +12V | LINE_A | Line current limit |
| R2 | 470 Ω ¼W | R2 470R | OPTO_A | GND | Shunt across PC817 LED (Rev 2.1; was 220 Ω in-series in legacy layout) |
| R3 | 10 kΩ ¼W | R3 10K | OPTO_EMIT | GND | Opto output pull-down |
| R7 | 2.2 kΩ ¼W | R7 2K2 | LINE_B | OPTO_LIM | Opto LED leg current limit — MANDATORY before 48 V (Rev 2.1) |
| C1 | 100 nF | C1 100nF | +5V | GND | L293D / logic decoupling |
| C2 | 100 nF | C2 100nF | +5V | GND | L293D / logic decoupling |
| C3 (Cc) | 10 µF 63V | Cc 10µF | LINE_A | XFMR_SEC | DC-block coupling cap in series with transformer secondary (Rev 2.1) |
| D1 | 1N4007 | D1 | OPTO_LIM | OPTO_A | Series reverse-block in opto LED leg (Rev 2.1) |
| D2–D5 | 1N4007 | D2–D5 | — | — | Transformer-primary overvoltage clamps (Rev 2.1) |

### Legacy auto-generated layout only — NOT on the Rev 2.1 master

The old `generate_*.py` output placed an **on-board LM2596 buck** (`U4` and its
passives) and used the reference designators `C2`, `C3` and `D1` for those parts.
The as-built Rev 2.1 master does **not** place `U4`/`L1` or the buck electrolytics —
the 12 V→5 V regulation is done off-board — so on the master `C2`/`C3`/`D1` are
**re-used** by the Rev 2.1 parts in the table above. These rows are documented only
so old Gerbers/BOMs still make sense — do **not** populate them on the master board.

| Ref (legacy) | Value | Net 1 | Net 2 | Function |
|-----|-------|-------|-------|----------|
| C2 (legacy) | 680 µF 25V | +12V | GND | LM2596 input filter (off-board on the master) |
| C3 (legacy) | 220 µF 25V | +5V | GND | LM2596 output filter (off-board on the master) |
| L1 (legacy) | 33 µH 3A | SW_OUT | +5V | Buck inductor (off-board on the master) |
| D1 (legacy) | 1N5825 | GND | SW_OUT | Buck freewheeling diode (off-board on the master) |

## Module Clearance Zones

Dashed silkscreen outlines on the PCB show where plug-in module boards sit.
Do not place tall components within these zones.

| Module | Size (mm) | Header | Orientation |
|--------|-----------|--------|-------------|
| 48 V input (was XL6009 Boost) | 43 × 21 | J2 | Dedicated 48 V adapter in Rev 2/2.1 |
| MAX98357A DAC | 19 × 18 | J5 (7-pin) | Extends left from header |
| Micro-SD Card | 25 × 20 | J7 (6-pin) | Extends right from header |
