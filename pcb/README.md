# PCB -- K6 GPO Exhibit

KiCad 10 design files for both the carrier board and optional A+B coin
box daughter board.  All components are through-hole -- no SMD soldering
required.

---

## Carrier Board

100 x 100 mm, 2-layer board.  All modules (ESP32 DevKit, MAX98357A,
SD card, LM2596 buck converter) plug in via pin headers.

### Files

| File | Description |
|------|-------------|
| `k6_carrier_rev2.kicad_pro` | KiCad project |
| `k6_carrier_rev2.kicad_sch` | Schematic (net and pin assignments) |
| `k6_carrier_rev2.kicad_pcb` | PCB layout with component placement, ground pour, board outline |
| `generate_pcb.py` | Python script that regenerates the carrier board PCB |
| `generate_schematic.py` | Python script that regenerates the schematic |
| `generate_kicad.py` | Legacy generator (original board layout) |
| `PINOUT.md` | Complete pinout reference for every connector and IC |
| `gerbers/` | Pre-generated Gerber and drill files, ready for fabrication |

### Board Specifications

| Parameter | Value |
|-----------|-------|
| Dimensions | 100 x 100 mm |
| Layers | 2 (F.Cu + B.Cu) |
| Copper weight | 1 oz |
| Board thickness | 1.6 mm |
| Mounting holes | 4x M3, 4 mm from edges |
| Ground pour | B.Cu (full back copper pour on GND net) |

### Trace Width Guide

| Net category | Width | Nets |
|--------------|-------|------|
| High voltage (48V) | 0.75-1.0 mm | +48V, BELL, LINE_B |
| Power rails | 0.5 mm | +12V, +5V, +3V3 |
| Signal | 0.25 mm | GPIOs, I2S, SPI |
| GND | n/a | Handled by B.Cu ground pour |

Maintain 0.5 mm minimum clearance between 48V traces and logic traces.

### Board Zones

```
+---------------------------------------------------------------+
|  ZONE A: POWER                                                 |
|  12V barrel jack, LM2596 buck module, 48V barrel jack          |
+---------------------------------------------------------------+
|  ZONE B: ESP32 + DEV ACCESS                                    |
|  ESP32 DevKit V1 (30-pin) with dev access holes on each pin    |
+---------------------------------------------------------------+
|  ZONE C: PERIPHERALS              | HIGH VOLTAGE               |
|  MAX98357A DAC, SD card module,    | L293D H-bridge (DIP-16)    |
|  audio transformer, PC817 opto,    | 48V traces, bell output    |
|  R1, R2, R3                       |                            |
+---------------------------------------------------------------+
|  ZONE D: PROTECTION DIODES                                     |
|  D1 (PC817 clamp), D2-D5 (DAC overvoltage clamps)             |
+---------------------------------------------------------------+
|  ZONE E: CONNECTORS                                            |
|  3-way phone terminal, 4x button headers, lamp connector,      |
|  R4/R5/R6 pull-ups, 6-pin daughter board header                |
+---------------------------------------------------------------+
```

### Connectors

| Ref | Description | Pins |
|-----|-------------|------|
| J1 | 12V DC power input | 2: +12V, GND |
| J2 | 48V DC power input | 2: +48V, GND |
| J_PHONE | Phone cord (screw terminal) | 3: Line A, Line B, Bell |
| J_ESP_L / J_ESP_R | ESP32 DevKit V1 sockets | 2x15 pin headers |
| J_DEV_L / J_DEV_R | Dev access holes | 2x15 (one per ESP32 pin) |
| U_DAC | MAX98357A DAC module | 7: VIN, GND, SD, GAIN, DIN, BCLK, LRC |
| J_SPK | DAC audio output (to transformer) | 2: L+, L- |
| U_SD | SD card module (SPI) | 6: 3V3, CS, MOSI, CLK, MISO, GND |
| U_BUCK | LM2596 buck converter module | 3+: IN+, IN-, OUT+, OUT- |
| J_RING | RING button | 2: signal, GND |
| J_CANCEL | CANCEL button | 2: signal, GND |
| J_RESET | RESET button | 2: signal, GND |
| J_COIN | Daughter board header | 6: GPIO36, GPIO39, GPIO35, +3V3, GND, +5V |

### ICs

| Ref | Part | Package |
|-----|------|---------|
| U1 | ESP32 DevKit V1 | 2x15 pin header socket (25.4 mm row spacing) |
| U2 | L293D H-bridge | DIP-16 (use IC socket) |
| U3 | PC817 optocoupler | DIP-4 (use IC socket) |

### Protection Diodes

| Ref | Part | Purpose |
|-----|------|---------|
| D1 | 1N4007 | Anti-parallel across PC817 LED (reverse voltage clamp) |
| D2 | 1N4007 | DAC_LP positive clamp (anode to DAC_LP, cathode to +5V) |
| D3 | 1N4007 | DAC_LP negative clamp (anode to GND, cathode to DAC_LP) |
| D4 | 1N4007 | DAC_LN positive clamp (anode to DAC_LN, cathode to +5V) |
| D5 | 1N4007 | DAC_LN negative clamp (anode to GND, cathode to DAC_LN) |

---

## Daughter Board (A+B Coin Box)

45 x 35 mm, 2-layer board.  Three optocoupler-isolated channels for
connecting any GPO A+B coin collecting box mechanism.

### Files

| File | Description |
|------|-------------|
| `k6_coinbox_daughter.kicad_pro` | KiCad project |
| `k6_coinbox_daughter.kicad_sch` | Schematic |
| `k6_coinbox_daughter.kicad_pcb` | PCB layout |
| `generate_coinbox_daughter.py` | Python script that regenerates all daughter board files |
| `COINBOX_DAUGHTER.md` | Full circuit description, wiring guide, and component list |

### Components

| Ref | Part | Purpose |
|-----|------|---------|
| U1-U3 | PC817 / EL817 (DIP-4) | Optocoupler isolation per channel |
| R1-R3 | 470 ohm 1/4W | Optocoupler LED current limit |
| R4-R6 | 10K 1/4W | Output pull-up resistors |
| J1-J3 | 2-pos screw terminal (5.08 mm) | A+B box contact inputs |
| J4 | 1x6 pin header (2.54 mm) | Output to carrier board |

See [`COINBOX_DAUGHTER.md`](COINBOX_DAUGHTER.md) for the full circuit
description, pin mapping, and A+B box wiring instructions.

---

## Fabrication

### Ordering from JLCPCB / PCBWay

1. Zip the `gerbers/` folder and upload to your fabricator
2. Select: 2-layer, 1.6 mm thickness, 1 oz copper, HASL finish
3. Board dimensions: 100 x 100 mm (carrier) or 45 x 35 mm (daughter)
4. Typical 5-board order: ~$2 + shipping

### Regenerating Gerbers from KiCad

1. Open `k6_carrier_rev2.kicad_pcb` in KiCad 10+
2. **File -> Plot** -> select Gerber format, output to `gerbers/`
3. Check layers: F.Cu, B.Cu, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts
4. **Generate Drill Files** -> Excellon format
5. Zip the `gerbers/` folder

Or run the export script:

```bash
cd pcb
python3 export_gerbers.py
```

### Assembly Notes

1. Solder IC sockets first (L293D DIP-16, PC817 DIP-4)
2. Solder resistors, capacitors, and diodes (D1-D5)
3. Solder pin headers for all module connectors
4. Solder screw terminals (power inputs, phone cord)
5. Solder transformer T1
6. Insert ICs into sockets (check pin 1 orientation)
7. Plug in modules: ESP32, MAX98357A, SD card, buck converter

---

## Regenerating Board Files

If you modify the design, edit the generator scripts and run:

```bash
cd pcb

# Carrier board
python3 generate_pcb.py

# Daughter board
python3 generate_coinbox_daughter.py
```

This regenerates the KiCad project, schematic, and PCB files from scratch.
