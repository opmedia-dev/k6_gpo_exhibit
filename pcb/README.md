# PCB — K6 GPO Exhibit Carrier Board

KiCad 7+ project for a 2-layer carrier board (100 × 80 mm).  All modules
plug in via through-hole pin headers — no SMD soldering required.

## Files

| File | Description |
|------|-------------|
| `k6_gpo_carrier.kicad_pro` | KiCad project file |
| `k6_gpo_carrier.kicad_sch` | Schematic (text-documentation style — shows all nets and pin assignments) |
| `k6_gpo_carrier.kicad_pcb` | PCB layout with component placement, ground-fill zones, and board outline |
| `generate_kicad.py` | Python script that generates all files above (run to regenerate) |

## Board Specifications

| Parameter | Value |
|-----------|-------|
| Dimensions | 100 × 80 mm |
| Layers | 2 (F.Cu + B.Cu) |
| Copper weight | 1 oz |
| Board thickness | 1.6 mm |
| Mounting holes | 4× M3, 4 mm from edges |
| Min trace width | 0.5 mm (power) / 0.25 mm (signal) |
| Min drill | 0.4 mm (vias), 0.8 mm (IC pads), 1.0 mm (headers) |

## Component Placement

```
┌──────────────────────────────────────────────────────────────────┐
│ [12V IN]  [BUCK 12V→5V]  [BOOST 12V→50V]         [MAX98357A]   │
│  J3          J1              J2              J5 DAC  J6 OUT     │
│                                                                  │
│                                              ┌──────────┐ [XFMR]│
│                                              │          │  T1    │
│ [BUTTONS]     ┌─────────────────────┐        │  PHONE   │       │
│  J8 RING      │                     │        │  J4      │       │
│  J9 CANCEL    │    ESP32 DevKit     │        │  3-pin   │       │
│  J10 RESET    │        U1           │        │  screw   │       │
│               │  2×15 pin headers   │        │          │       │
│               │                     │        └──────────┘       │
│               └─────────────────────┘   [C1]                    │
│                                              ┌────────┐         │
│ [SD CARD]      [R1 470R]  [R2 220R]         │ L293D  │ [R3 10K]│
│  J7            ──────────  ──────────        │  U2    │ ────────│
│                                              └────────┘         │
│                                              ┌────┐             │
│                                              │PC817│            │
│                                              │ U3  │            │
│ (MH)                                         └────┘       (MH) │
└──────────────────────────────────────────────────────────────────┘
  (MH) = Mounting hole M3                                    (MH)
```

## Connectors

| Ref | Description | Pins |
|-----|-------------|------|
| J1 | LM2596 buck converter (12V → 5V) | 4: IN+, IN-, OUT+, OUT- |
| J2 | XL6009 boost converter (12V → 50V) | 4: IN+, IN-, OUT+, OUT- |
| J3 | 12V DC power input | 2: +12V, GND |
| J4 | Phone cord (3-pin screw terminal) | 3: Line A, Line B, Bell |
| J5 | MAX98357A DAC module | 7: VIN, GND, SD, GAIN, DIN, BCLK, LRC |
| J6 | DAC audio output (to transformer) | 2: L+, L- |
| J7 | SD card module (SPI) | 6: GND, VCC, MOSI, MISO, SCK, CS |
| J8 | RING button (to control panel) | 2: signal, GND |
| J9 | CANCEL button (to control panel) | 2: signal, GND |
| J10 | RESET button (to control panel) | 2: signal, GND |

## ICs

| Ref | Part | Package |
|-----|------|---------|
| U1 | ESP32 DevKit V1 | 2×15 pin header socket (25.4 mm row spacing) |
| U2 | L293D H-bridge | DIP-16 (use IC socket) |
| U3 | PC817 optocoupler | DIP-4 (use IC socket) |

## Passive Components

| Ref | Value | Purpose |
|-----|-------|---------|
| R1 | 470 Ω 1W | Line current limit |
| R2 | 220 Ω ¼W | Optocoupler LED current limit |
| R3 | 10 kΩ ¼W | Optocoupler output pull-down |
| C1 | 100 nF | L293D decoupling capacitor |
| T1 | 600 Ω : 600 Ω transformer | Audio coupling to phone line |

## Fabrication

### Ordering from JLCPCB / PCBWay

1. Open the project in KiCad 7+
2. **File → Plot** → select Gerber format, output to `gerbers/`
3. Check layers: F.Cu, B.Cu, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts
4. **Generate Drill Files** → Excellon format
5. Zip the `gerbers/` folder and upload to your fabricator

Typical 5-board order from JLCPCB: ~$2 + shipping.

### Assembly Notes

1. Solder IC sockets first (U2 DIP-16, U3 DIP-4)
2. Solder resistors and capacitor
3. Solder pin headers for all module connectors
4. Solder screw terminals (J3 power, J4 phone)
5. Solder transformer T1
6. Insert ICs into sockets (mind pin 1 orientation)
7. Plug in modules: ESP32, MAX98357A, SD card, buck converter, boost converter

## Regenerating Files

If you modify the board layout, edit `generate_kicad.py` and run:

```bash
cd pcb
python3 generate_kicad.py
```

This regenerates all three KiCad files from scratch.
