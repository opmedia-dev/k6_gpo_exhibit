# K6 GPO Exhibit — Carrier Board Pinout Reference

Full pinout for every connector and IC on the carrier board. Silkscreen
labels on the PCB use short abbreviations — this document provides the
complete mapping.

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

TO-220-5 package, on-board with supporting passives.

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

## J2 — XL6009 Boost Converter (12V → 50V)

4-pin header

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | 12V+ | +12V | Input positive |
| 2 | GND | GND | Input negative |
| 3 | 50V+ | +50V | Output positive (adjust trimpot) |
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
| 8 | L | 50V | +50V | Motor supply (from boost) |
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
| 1 | L | An | OPTO_ANODE | LED anode (from R2) |
| 2 | L | Kth | LINE_B | LED cathode → Line B |
| 3 | R | Em | OPTO_EMIT | Phototransistor emitter → R3 → GND |
| 4 | R | Col | +3V3 | Phototransistor collector → 3.3V |

**Hook detect circuit:** +12V → R1 (470 Ω) → junction → R2 (220 Ω) → PC817 anode (pin 1). PC817 cathode (pin 2) → LINE_B. When phone is off-hook, current flows through the loop, illuminating the optocoupler LED. The phototransistor output (pin 3) is read by ESP32 GPIO 34 (ADC).

## Passive Components

| Ref | Value | Silk | Net 1 | Net 2 | Function |
|-----|-------|------|-------|-------|----------|
| R1 | 470 Ω 1W | R1 470R | +12V | JUNC_A | Line current limit |
| R2 | 220 Ω ¼W | R2 220R | JUNC_A | OPTO_ANODE | Opto LED current limit |
| R3 | 10 kΩ ¼W | R3 10K | OPTO_EMIT | GND | Opto output pull-down |
| C1 | 100 nF | C1 100nF | +5V | GND | L293D decoupling |
| C2 | 680 µF 25V | C2 680µF | +12V | GND | LM2596 input filter |
| C3 | 220 µF 25V | C3 220µF | +5V | GND | LM2596 output filter |
| L1 | 33 µH 3A | L1 33µH | SW_OUT | +5V | Buck inductor |
| D1 | 1N5825 | D1 1N5825 | GND | SW_OUT | Buck freewheeling diode |

## Module Clearance Zones

Dashed silkscreen outlines on the PCB show where plug-in module boards sit.
Do not place tall components within these zones.

| Module | Size (mm) | Header | Orientation |
|--------|-----------|--------|-------------|
| XL6009 Boost | 43 × 21 | J2 (4-pin) | Extends right from header |
| MAX98357A DAC | 19 × 18 | J5 (7-pin) | Extends left from header |
| Micro-SD Card | 25 × 20 | J7 (6-pin) | Extends right from header |
