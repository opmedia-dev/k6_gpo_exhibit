# Bill of Materials (Rev 2)

> **Revision 2** — updated for dual DC supply (12 V + 48 V), four
> control buttons, external pull-ups, and panel lamp.

## Power Supply

| Qty | Component | Value / Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 1 | 12 V DC adapter | 12 V / 1 A, barrel-jack wall-wart | Logic power | £5 |
| 1 | 48 V DC adapter | 48 V / 1 A, barrel-jack wall-wart | Bell power | £10-15 |
| 1 | Buck converter module | LM2596 adjustable (set to 5 V) | 12 V → 5 V for ESP32 and peripherals | £1-2 |

> **Rev 2 change:** The XL6009 boost converter has been removed. Bell
> power comes directly from the 48 V adapter, eliminating the component
> that caused voltage dips and ESP32 brownouts during ringing.

## Controller & Audio

| Qty | Component | Value / Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 1 | ESP32 DevKit V1 | ESP-WROOM-32 | Main controller | £6 |
| 1 | MAX98357A I2S DAC module | — | Audio output to phone earpiece | £2 |
| 1 | Micro-SD card module | SPI breakout | MP3 file storage | £1 |
| 1 | Micro-SD card | ≥ 1 GB, FAT32 | Stores audio files and settings | £3 |

## Phone Line Interface

| Qty | Component | Value / Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 1 | Optocoupler | PC817 (DIP-4) | Hook/dial isolation + detection | £0.30 |
| 1 | H-bridge motor driver | L293D (DIP-16) | Bell ring generator | £1.50 |
| 1 | Audio transformer | 600 Ω : 600 Ω 1:1 | Phone line audio coupling | £2 |

## Passive Components

| Qty | Component | Value | Label | Purpose |
|-----|-----------|-------|-------|---------|
| 1 | Resistor | 470 Ω, 1 W | R1 | Line current limit (12 V to phone) |
| 1 | Resistor | 220 Ω, ¼ W | R2 | Optocoupler LED current limit |
| 1 | Resistor | 10 kΩ, ¼ W | R3 | Pull-down for optocoupler output (GPIO 34) |
| 3 | Resistor | 10 kΩ, ¼ W | R4, R5, R6 | **Pull-ups for coin box GPIOs (36, 39, 35)** |
| 2 | Capacitor (ceramic) | 100 nF | C1, C2 | Decoupling: C1 at L293D VSS, C2 at ESP32 VIN |

> **Rev 2 change:** Added R4/R5/R6 pull-up resistors. These are
> essential — GPIO 36/39/35 are input-only and have no internal
> pull-up. Without these, the firmware falsely detects an A+B coin
> box. Also removed the LM2596 discrete components (inductor, Schottky
> diode, electrolytic caps) since we now use a pre-built module.

## Control Panel

| Qty | Component | Notes |
|-----|-----------|-------|
| 4 | Momentary push buttons | RING / CANCEL / RESET / MODE |
| 1 | Panel lamp | 3-6 V LED or filament indicator lamp |

> **Rev 2 change:** Added MODE button (GPIO 14) — was missing from
> Rev 1 BOM but required by firmware. Added panel lamp.

## Connectors & Mechanical

| Qty | Component | Notes |
|-----|-----------|-------|
| 1 | 3-way screw terminal block | Phone cord connection (Line A, Line B, Bell) |
| 2 | Barrel jack sockets (2.1 mm) | 12 V and 48 V power inputs |
| 1 | 6-pin header (2.54 mm) | A+B coin box daughter board connector |
| 2 | 19-pin female headers | ESP32 DevKit sockets (allows removal) |
| 1 | Project box / enclosure | Houses all electronics + buttons |
| 1 | Multi-core cable (≥ 6 conductors) | Extended lead from phone to enclosure |
| 1 | Custom carrier PCB | 100 × 80 mm, 2-layer |
| — | Hook-up wire, solder, standoffs | Assembly |

## Optional — A+B Coin Box Daughter Board

Small add-on board for connecting a GPO A+B coin collecting box mechanism.
Plugs into the 6-pin header on the carrier board.

| Qty | Component | Value / Part | Purpose | Approx. Cost |
|-----|-----------|-------------|---------|-------------|
| 3 | Optocoupler (DIP-4) | PC817 / EL817 | Isolate A+B box contacts from ESP32 | £0.90 |
| 3 | Resistor | 470 Ω, ¼ W | Optocoupler LED current limit | £0.10 |
| 3 | Screw terminal | 2-pos, 5.08 mm pitch | A+B box contact inputs | £0.60 |
| 1 | Pin header | 1×6, 2.54 mm | Output to carrier board 6-pin header | £0.10 |
| 1 | Daughter board PCB | 45 × 35 mm | — | £2 |
| — | Hook-up wire | 22–26 AWG | A+B box to daughter board wiring | £1 |
| | | | **Daughter board subtotal** | **~£5** |

> **Rev 2 change:** The 10 kΩ pull-up resistors that were on the
> daughter board in Rev 1 have been moved to the **carrier board**
> (R4/R5/R6). This ensures the pull-ups are always present even when
> the daughter board is not connected.

## Optional — Other

| Qty | Component | Purpose |
|-----|-----------|---------|
| 1 | Electromagnetic bell (Bellset No. 26) | External bell for GPO 232 |
| 1 | USB Micro-B cable | For reprogramming / serial debug |
| 1 | 3D-printed enclosure | Tidier than a generic project box |
| 1 | 2N2222 NPN transistor | For driving a filament panel lamp (not needed for LED) |
| 1 | 1 kΩ resistor | Base resistor for 2N2222 (if using filament lamp) |
| 1 | 100 Ω resistor | Series resistor for LED panel lamp (if using LED) |

## Total Estimated Cost

| Category | Cost |
|----------|------|
| Power supply (12 V adapter + 48 V adapter + buck module) | ~£18 |
| Controller + audio (ESP32 + DAC + SD) | ~£12 |
| Phone line interface (opto + H-bridge + transformer) | ~£4 |
| Passives (resistors, capacitors) | ~£2 |
| Connectors, buttons, lamp | ~£5 |
| **Total** | **~£41** |

*Excludes: phone, SD card content, enclosure, extended cable.*

## Where to Buy (UK)

- **Amazon UK** — ESP32 DevKit, LM2596 modules, SD card modules, 48 V adapters
- **RS Components / Mouser** — L293D, resistors, capacitors, barrel jack sockets
- **eBay** — PC817 optocouplers, 600 Ω audio transformers, push buttons, 48 V PSUs
- **CPC/Farnell** — Resistors, capacitors, screw terminals, headers
- **AliExpress** — Cheapest for modules (longer delivery)
- **Pimoroni / The Pi Hut** — ESP32 boards, breakout modules

## Rev 1 → Rev 2 Component Changes Summary

| Removed | Added | Reason |
|---------|-------|--------|
| XL6009 boost converter module | 48 V DC adapter | Direct bell power, no boost needed |
| LM2596 bare IC + inductor + Schottky + caps | LM2596 pre-built module | Simpler, fewer discrete components |
| — | 3× 10 kΩ pull-up resistors (R4/R5/R6) | Fix false A+B detection |
| — | MODE button (4th button) | Was in firmware but missing from BOM |
| — | Panel lamp + driver components | Auto-mode status indicator |
| — | 2nd barrel jack socket | Separate 48 V power input |
| — | 6-pin header | Standardised daughter board connector |
| — | 2× 19-pin female headers | ESP32 socket (allows removal for dev) |
| — | 100 nF decoupling cap (C2) | Noise filtering at ESP32 VIN |
