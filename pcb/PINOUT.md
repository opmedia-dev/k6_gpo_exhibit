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

## J1 -- 12V DC Power Input

2-pin screw terminal (barrel jack)

| Pin | Net | Description |
|-----|-----|-------------|
| 1 | +12V | DC input from 12V adapter |
| 2 | GND | Ground |

## U_BUCK -- LM2596 Buck Converter Module (12V -> 5V)

Pre-built adjustable module.  Set the trimpot to 5.0V output before
connecting other components.

| Pin | Net | Description |
|-----|-----|-------------|
| IN+ | +12V | Input voltage (12V) |
| IN- | GND | Input ground |
| OUT+ | +5V | Output voltage (set to 5.0V) |
| OUT- | GND | Output ground |

## J2 -- 48V DC Power Input

2-pin screw terminal (barrel jack)

| Pin | Net | Description |
|-----|-----|-------------|
| 1 | +48V | DC input from 48V adapter (bell power) |
| 2 | GND | Ground |

## U1 -- ESP32 DevKit V1

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

## U_DAC -- MAX98357A I2S DAC Module

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

## J_SPK -- DAC Audio Output

2-pin header (connects to transformer T1 primary)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | SPK+ | AUDIO_P1 | Speaker output + |
| 2 | SPK- | AUDIO_P2 | Speaker output − |

## T1 -- Audio Coupling Transformer

600 Ω : 600 Ω, 4-pin

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | SPK+ | AUDIO_P1 | Primary (from DAC SPK+) |
| 2 | SPK- | AUDIO_P2 | Primary (from DAC SPK−) |
| 3 | LnA | LINE_A | Secondary → phone Line A |
| 4 | LnB | LINE_B | Secondary → phone Line B |

## J_PHONE -- Phone Cord

3-pin screw terminal (rotated 90°, pitch 5.08 mm)

| Pin | Silk | Net | Wire Colour | Description |
|-----|------|-----|-------------|-------------|
| 1 | A Red | LINE_A | Red | Line A |
| 2 | B Wht | LINE_B | White | Line B |
| 3 | Bell | BELL | Blue | Bell coil connection |

## J_RING -- Ring Button

2-pin header (active low, internal pull-up)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | RING | BTN_RING | Button signal → ESP32 GPIO 32 |
| 2 | GND | GND | Ground |

## J_CANCEL -- Cancel Button

2-pin header (active low, internal pull-up)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | CANCEL | BTN_CANCEL | Button signal → ESP32 GPIO 33 |
| 2 | GND | GND | Ground |

## J_RESET -- Reset Button

2-pin header (active low, internal pull-up)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | RESET | BTN_RESET | Button signal → ESP32 GPIO 27 |
| 2 | GND | GND | Ground |

## U_SD -- Micro-SD Card Module (SPI)

6-pin header (pin order matches common breakout modules)

| Pin | Silk | Net | Description |
|-----|------|-----|-------------|
| 1 | 3V3 | +3V3 | Module power (3.3V) |
| 2 | CS | SD_CS | Chip select -> ESP32 GPIO 5 |
| 3 | MOSI | SPI_MOSI | Data out -> ESP32 GPIO 23 |
| 4 | CLK | SPI_SCK | SPI clock -> ESP32 GPIO 18 |
| 5 | MISO | SPI_MISO | Data in <- ESP32 GPIO 19 |
| 6 | GND | GND | Ground |

## U2 -- L293D H-Bridge Motor Driver (DIP-16)

| Pin | Side | Silk | Net | Description |
|-----|------|------|-----|-------------|
| 1 | L | EN | RING_EN | Enable 1/2 -> ESP32 GPIO 4 |
| 2 | L | IN1 | RING_A | Input 1 -> ESP32 GPIO 16 |
| 3 | L | BELL | BELL | Output 1 -> bell coil |
| 4 | L | GND | GND | Ground |
| 5 | L | GND | GND | Ground |
| 6 | L | LnB | LINE_B | Output 2 -> phone Line B |
| 7 | L | IN2 | RING_B | Input 2 -> ESP32 GPIO 17 |
| 8 | L | 48V | +48V | Motor supply (48V bell power) |
| 9 | R | GND | GND | Ground |
| 10 | R | -- | -- | Not connected |
| 11 | R | -- | -- | Not connected |
| 12 | R | -- | GND | Ground |
| 13 | R | -- | GND | Ground |
| 14 | R | -- | -- | Not connected |
| 15 | R | -- | +5V | Logic supply |
| 16 | R | 5V | +5V | Logic supply |

## U3 -- PC817 Optocoupler (DIP-4)

| Pin | Side | Silk | Net | Description |
|-----|------|------|-----|-------------|
| 1 | L | An | OPTO_ANODE | LED anode (from R2) |
| 2 | L | Kth | LINE_B | LED cathode → Line B |
| 3 | R | Em | OPTO_EMIT | Phototransistor emitter → R3 → GND |
| 4 | R | Col | +3V3 | Phototransistor collector → 3.3V |

**Hook detect circuit:** +12V -> R1 (470 ohm) -> junction -> R2 (220 ohm) -> PC817 anode (pin 1). PC817 cathode (pin 2) -> LINE_B. When phone is off-hook, current flows through the loop, illuminating the optocoupler LED. The phototransistor output (pin 3) is read by ESP32 GPIO 34 (ADC).

## Protection Diodes

| Ref | Part | Net 1 | Net 2 | Purpose |
|-----|------|-------|-------|--------|
| D1 | 1N4007 | LINE_B (anode) | OPTO_A (cathode) | Anti-parallel across PC817 LED -- clamps reverse voltage during ringing |
| D2 | 1N4007 | DAC_LP (anode) | +5V (cathode) | Positive overvoltage clamp on transformer primary L+ |
| D3 | 1N4007 | GND (anode) | DAC_LP (cathode) | Negative overvoltage clamp on transformer primary L+ |
| D4 | 1N4007 | DAC_LN (anode) | +5V (cathode) | Positive overvoltage clamp on transformer primary L- |
| D5 | 1N4007 | GND (anode) | DAC_LN (cathode) | Negative overvoltage clamp on transformer primary L- |

## J_COIN -- Daughter Board Header

6-pin header for connecting the optional A+B coin box daughter board.

| Pin | Net | Description |
|-----|-----|-------------|
| 1 | GPIO 36 | Coin sense (input-only, external pull-up R4 to +3V3) |
| 2 | GPIO 39 | Button A (input-only, external pull-up R5 to +3V3) |
| 3 | GPIO 35 | Button B (input-only, external pull-up R6 to +3V3) |
| 4 | +3V3 | 3.3V power for daughter board pull-ups |
| 5 | GND | Ground |
| 6 | +5V | 5V power for daughter board optocoupler LED drive |

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

## Pull-Up Resistors (Coin Box GPIOs)

| Ref | Value | GPIO | Net |
|-----|-------|------|-----|
| R4 | 10K | 36 | +3V3 -> GPIO 36 |
| R5 | 10K | 39 | +3V3 -> GPIO 39 |
| R6 | 10K | 35 | +3V3 -> GPIO 35 |

These are required even without the daughter board.  GPIO 36/39/35 are
input-only pins with no internal pull-up on the ESP32.  Without these
resistors the firmware falsely detects an A+B coin box at boot.
