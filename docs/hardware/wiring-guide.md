# Wiring Guide

## Before You Start

1. **Identify your phone model.**
   - GPO 232 — no internal bell (needs external Bellset No. 26)
   - GPO 332 — internal bell + 2 µF capacitor
2. **Check the cord wires.** Wire colours may vary on restored phones.
   Use a multimeter:
   - **Line pair:** two wires that show ~150-200 Ω when the handset is lifted
   - **Bell wire:** the third wire, high impedance to both line wires when on-hook
3. **Do not modify the phone.** All connections are to the free cord ends only.
4. **The ESP32 board lives in a separate enclosure**, connected to the phone
   via an extended multi-core cable.

---

## Parts You Need

| # | Part | Notes |
|---|------|-------|
| 1 | ESP32 DevKit V1 | Main controller |
| 2 | 12 V DC adapter (≥ 1 A) | Barrel jack, wall-wart type |
| 3 | LM2596 buck converter module | Adjustable, set to 5 V output |
| 4 | XL6009 boost converter module | Adjustable, set to 50 V output |
| 5 | PC817 optocoupler | Hook/dial detection |
| 6 | L293D H-bridge (DIP-16) | Bell ring generator |
| 7 | MAX98357A I2S DAC module | Audio output |
| 8 | Micro-SD card breakout module | SPI interface |
| 9 | 600 Ω : 600 Ω audio transformer | 1:1, telephone line coupling |
| 10 | 470 Ω resistor (1 W) | R1 — line current limit |
| 11 | 220 Ω resistor (¼ W) | R2 — optocoupler LED limit |
| 12 | 10 kΩ resistor (¼ W) | R3 — pull-down on GPIO 34 |
| 13 | 3× momentary push buttons | RING / CANCEL / RESET |
| 14 | 3-way screw terminal block | Phone cord connection |
| 15 | Prototype PCB / stripboard | 80 × 60 mm minimum |
| 16 | Hook-up wire, solder | Assembly |

---

## Step 1: Set Up the Power Supplies

> **Do this first, before connecting any other components.**

### 1a. Buck converter (12 V → 5 V)

```
    12 V adapter (+) ──────► [LM2596 module IN+]
    12 V adapter (-) ──────► [LM2596 module IN-]

    Adjust trimpot until output reads 5.0 V on multimeter.

    [LM2596 module OUT+] = +5 V rail
    [LM2596 module OUT-] = GND rail
```

### 1b. Boost converter (12 V → 50 V)

```
    12 V adapter (+) ──────► [XL6009 module IN+]
    12 V adapter (-) ──────► [XL6009 module IN-]

    Adjust trimpot until output reads 50.0 V on multimeter.
    ⚠ CAUTION: 50 V can give a nasty tingle. Don't touch the output terminals.

    [XL6009 module OUT+] = +50 V rail (for bell only)
    [XL6009 module OUT-] = GND
```

---

## Step 2: Wire the ESP32 DevKit

Connect power to the ESP32:

```
    +5 V rail ──────► ESP32 VIN pin
    GND rail ───────► ESP32 GND pin
```

> **Note:** Do not connect both USB and 12 V at the same time unless you
> add a diode on VIN to prevent back-feeding.

---

## Step 3: Wire the Phone Cord Terminal Block

Strip ~5 mm from each wire of the 3-core cord:

```
    Terminal 1  ◄──  Red wire    (Line A)
    Terminal 2  ◄──  White wire  (Line B)
    Terminal 3  ◄──  Blue wire   (Bell)
```

---

## Step 4: Wire the Hook / Dial Detection Circuit

```
    +12 V ─── [R1 470 Ω 1W] ─── Junction A
    Junction A ─── Terminal 1 (Red / Line A)
    Junction A ─── [R2 220 Ω] ─── PC817 pin 1 (Anode)
    Terminal 2 (White / Line B) ─── PC817 pin 2 (Cathode)

    PC817 pin 4 (Collector) ─── +3.3 V (ESP32 3V3 pin)
    PC817 pin 3 (Emitter) ─── GPIO 34
    PC817 pin 3 (Emitter) ─── [R3 10 kΩ] ─── GND
```

**PC817 pinout:**

```
    ┌──────────┐
    │  1  Anode │──  from R2
    │  2 Cathode│──  to Terminal 2
    │  3 Emitter│──  to GPIO 34 + R3
    │  4 Collctr│──  to +3.3V
    └──────────┘
```

---

## Step 5: Wire the Ring Generator (L293D H-Bridge)

```
    L293D pin 1  (EN1,2)  ──── GPIO 4
    L293D pin 2  (IN1)    ──── GPIO 16
    L293D pin 7  (IN2)    ──── GPIO 17
    L293D pin 8  (VS)     ──── +50 V (from XL6009 boost converter)
    L293D pin 16 (VSS)    ──── +5 V
    L293D pin 3  (OUT1)   ──── Terminal 3 (Blue / Bell wire)
    L293D pin 6  (OUT2)   ──── Terminal 2 (White / Line B)
    L293D pins 4,5,12,13  ──── GND
```

**L293D pinout (DIP-16) — top view:**

```
          ┌────────┐
   EN1,2  │ 1   16 │  VSS (+5V)
     IN1  │ 2   15 │  EN3,4 (n/c)
    OUT1  │ 3   14 │  IN4 (n/c)
     GND  │ 4   13 │  GND
     GND  │ 5   12 │  GND
    OUT2  │ 6   11 │  OUT4 (n/c)
     IN2  │ 7   10 │  IN3 (n/c)
  VS(50V) │ 8    9 │  GND
          └────────┘
```

---

## Step 6: Wire the Audio Output (MAX98357A + Transformer)

```
    GPIO 26 ──── MAX98357A BCLK
    GPIO 25 ──── MAX98357A LRC
    GPIO 22 ──── MAX98357A DIN
    +5 V    ──── MAX98357A VIN
    GND     ──── MAX98357A GND

    MAX98357A L+ ──── Audio transformer PRIMARY pin 1
    MAX98357A L- ──── Audio transformer PRIMARY pin 2

    Audio transformer SECONDARY pin 1 ──── Terminal 1 (Red / Line A)
    Audio transformer SECONDARY pin 2 ──── Terminal 2 (White / Line B)
```

> If audio is too loud or distorted, add a **10 Ω resistor** in series
> between MAX98357A L+ and the transformer primary.

---

## Step 7: Wire the SD Card Module

```
    GPIO 5  ──── SD module CS
    GPIO 23 ──── SD module MOSI (DI)
    GPIO 19 ──── SD module MISO (DO)
    GPIO 18 ──── SD module SCK (CLK)
    +3.3 V  ──── SD module VCC
    GND     ──── SD module GND
```

> Most SD card modules have an on-board 3.3 V regulator, so you can
> safely connect VCC to 3.3 V or 5 V depending on the module.  Check
> your module's documentation.

---

## Step 8: Wire the Control Panel Buttons

Three momentary push buttons, each wired between the GPIO pin and GND.
No external resistors — the ESP32 enables internal pull-ups.

```
    GPIO 32 ──── [RING button]   ──── GND
    GPIO 33 ──── [CANCEL button] ──── GND
    GPIO 27 ──── [RESET button]  ──── GND
```

Mount these buttons on the lid or front panel of the enclosure.

---

## Step 9: Prepare the SD Card

1. Format a micro-SD card as **FAT32**
2. Create directories and add MP3 files:

```
    /system/
        dialtone.mp3             ← Continuous dial tone (will loop)
        busy.mp3                 ← Busy/error tone (will loop)
        not_recognised.mp3       ← "The number you have dialled..."
    /history/
        001.mp3                  ← History tracks (random pick on answer)
        002.mp3
        003.mp3
    /numbers/
        999.mp3                  ← Plays when user dials 999
        100.mp3                  ← Plays when user dials 100
        08001111.mp3             ← Plays when user dials 08001111
```

3. Insert into the SD card module **before** powering on

---

## Testing Procedure

### Test 1: Power

1. Connect the 12 V adapter.  **Do not connect the phone yet.**
2. Verify with a multimeter:
   - Buck converter output = **5 V** (±0.2 V)
   - Boost converter output = **50 V** (±2 V)
3. Confirm the ESP32 boots — serial monitor (115200 baud) should show:
   ```
   K6 GPO Exhibit — ESP32 Phone Interface
   [audio] SD card ready
   [phone] controller ready
   ```

### Test 2: SD Card

1. Send `S` via serial.  Check output shows `sd=OK`.
2. If `sd=FAIL`, check SPI wiring and card format.

### Test 3: Hook Detection

1. Connect the phone's 3-core cord to the terminal block.
2. With handset **on the cradle**, send `S` — should show `hook=ON_HOOK`.
3. **Lift the handset** — serial should print:
   ```
   [app] hook: OFF_HOOK
   [phone] → DIAL_TONE
   ```
   You should hear the dial tone in the earpiece.

### Test 4: Rotary Dialling

1. With handset lifted, dial digit **5**.
2. Serial should print `[phone] digit: 5`.
3. Wait 3 seconds — if `/numbers/5.mp3` exists it plays, otherwise
   you'll hear "number not recognised".

### Test 5: Bell Ringing

1. Replace the handset (on-hook).
2. Send `R` via serial **or** press the RING button.
3. The bell should ring with the UK cadence (ring-ring, pause).
4. Lift the handset — ringing stops, a random history track plays.

### Test 6: Control Buttons

1. Press **CANCEL** during ringing → ringing should stop.
2. Press **RESET** → ESP32 reboots (serial shows boot messages again).

---

## Troubleshooting

| Symptom | What to check |
|---------|--------------|
| ESP32 doesn't boot | Check 5 V rail with multimeter. Ensure VIN, not 3.3V pin, is connected. |
| No hook detection | Measure voltage across R2 with handset lifted — should be ~1-2 V. Check optocoupler pin orientation. |
| Dial pulses not counted | Check R1/R2 values give ~15-25 mA off-hook. Adjust `LINE_THRESHOLD_ON/OFF` in `config.h`. |
| Bell doesn't ring | Verify 50 V on boost output. Check all 4 GND pins on L293D. GPO 232 needs an external bellset. |
| No audio / no sound | Check MAX98357A wiring (BCLK, LRC, DIN). Try `V9` serial command for max volume. Check transformer orientation. |
| Audio distorted | Add 10 Ω series resistor on transformer primary. Reduce volume with `V3` or similar. |
| SD card not detected | Check SPI wiring (CS, MOSI, MISO, SCK). Ensure card is FAT32 formatted. Try a different card. |
| MP3 doesn't play | Check file paths match exactly (`/system/`, `/history/`, `/numbers/`). Ensure valid MP3 encoding. |
| Buttons don't work | Check wiring to GND. Verify correct GPIO numbers. Use `S` command to see current state. |
| ESP32 resets during ring | 50 V boost may cause voltage dips. Add 470 µF capacitor on 5 V rail. Use separate GND path for boost. |
