# Bill of Materials

## Core Module

| Qty | Component | Value / Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 1 | ESP32 DevKit V1 | ESP-WROOM-32 | Main controller | £6 |
| 1 | Optocoupler | PC817 (or similar) | Line sense / isolation | £0.30 |
| 1 | H-bridge motor driver | L293D (DIP-16) | Bell ring generator | £1.50 |
| 1 | Audio transformer | 600 Ω : 600 Ω 1:1 | Line audio coupling | £2 |
| 1 | Boost converter module | XL6009 / MT3608 | 5 V → 50 V for bell | £2 |
| 1 | Boost converter module | XL6009 or step-up | 5 V → 12 V for line supply | £2 |

## Passive Components

| Qty | Component | Value | Purpose |
|-----|-----------|-------|---------|
| 1 | Resistor | 470 Ω, 1 W | Line current limit (R1) |
| 1 | Resistor | 220 Ω, ¼ W | Optocoupler LED current limit (R2) |
| 1 | Resistor | 10 kΩ, ¼ W | Pull-down for optocoupler output |
| 2 | Capacitor (film) | 1 µF, 50 V | Audio coupling caps |
| 1 | Capacitor (electrolytic) | 100 µF, 63 V | Line supply filter |
| 1 | Capacitor (ceramic) | 100 nF | Decoupling for L293D |

## Connectors & Mechanical

| Qty | Component | Notes |
|-----|-----------|-------|
| 1 | Screw terminal block, 3-way | Phone cord connection (Line A, Line B, Bell) |
| 1 | USB Micro-B cable | ESP32 power & serial |
| 1 | Prototype PCB / stripboard | 70 × 50 mm minimum |
| — | Hook-up wire, solder | Assembly |

## Optional

| Qty | Component | Purpose |
|-----|-----------|---------|
| 1 | External 12 V DC adapter (≥ 500 mA) | Avoids needing a 5 V → 12 V boost |
| 1 | Electromagnetic bell (e.g. Bellset No. 26) | External bell for GPO 232 |
| 1 | I2S DAC module (MAX98357A) | Higher-quality earpiece audio |
| 1 | I2S microphone (INMP441) | Higher-quality mic capture |
| 1 | 3D-printed enclosure | Tidy installation inside K6 box |

## Total Estimated Cost

Core components: **~£14** (excluding phone and enclosure).
