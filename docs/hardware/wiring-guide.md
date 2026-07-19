# Wiring Guide (Rev 2)

> **Revision 2** — updated for dual DC supply, four buttons, external
> pull-ups, panel lamp, and 6-pin daughter board header.

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
| 1 | ESP32 DevKit V1 (30-pin) | Main controller, USB port faces board edge |
| 2 | 12 V DC adapter (≥ 1 A) | Barrel jack, logic power |
| 3 | 48 V DC adapter (≥ 1 A) | Barrel jack, bell power |
| 4 | LM2596 buck converter module | Adjustable, set to 5 V output |
| 5 | PC817 optocoupler | Hook/dial detection |
| 6 | L293D H-bridge (DIP-16) | Bell ring generator |
| 7 | MAX98357A I2S DAC module | Audio output |
| 8 | Micro-SD card breakout module | SPI interface |
| 9 | 600 Ω : 600 Ω audio transformer | 1:1, telephone line coupling |
| 10 | 470 Ω resistor (1 W) | R1 — line current limit |
| 11 | 1.5–2.2 kΩ resistor (¼ W) | R_LIM — opto LED leg current limit (Rev 2.1, replaces R2). Mandatory before 48 V |
| 11b | 10 µF ≥63 V non-polar cap (or 2× 10 µF 63 V electrolytic back-to-back) | Cc — DC-block coupling cap on transformer secondary (Rev 2.1) |
| 11c | 1N4148 diode | D1 — series in opto LED leg, blocks reverse ring (Rev 2.1) |
| 12 | 10 kΩ resistor (¼ W) | R3 — pull-down on GPIO 34 |
| 13 | 3× 10 kΩ resistors (¼ W) | R4, R5, R6 — pull-ups on GPIO 36/39/35 |
| 14 | 2× 100 nF ceramic capacitors | C1, C2 — decoupling |
| 15 | 4× momentary push buttons | RING / CANCEL / RESET / MODE |
| 16 | 1× panel lamp (LED or filament) | Auto-mode indicator |
| 17 | 3-way screw terminal block | Phone cord connection |
| 18 | 2× barrel jack sockets | 12 V and 48 V power inputs |
| 19 | 1× 6-pin header (2.54 mm) | A+B daughter board connector |
| 20 | Prototype PCB | 100 × 80 mm minimum |
| 21 | Hook-up wire, solder | Assembly |

---

## Step 1: Set Up the Power Supplies

> **Do this first, before connecting any other components.**
> **Rev 2 uses two separate power supplies.**

### 1a. Buck converter (12 V → 5 V)

```
    12 V adapter (+) ──────► [LM2596 module IN+]
    12 V adapter (-) ──────► [LM2596 module IN-]

    Adjust trimpot until output reads 5.0 V on multimeter.

    [LM2596 module OUT+] = +5 V rail
    [LM2596 module OUT-] = GND rail
```

### 1b. 48 V supply (bell power)

```
    48 V adapter (+) ──────► +48 V rail (goes to L293D pin 8 only)
    48 V adapter (-) ──────► GND rail

    ⚠ CAUTION: 48 V can give a nasty tingle. Don't touch the output.
```

### 1c. Common ground

```
    12 V GND ──────┐
                   ├──── Common GND bus on the board
    48 V GND ──────┘

    Connect these at a SINGLE POINT near the L293D GND pins.
```

---

## Step 2: Wire the ESP32 DevKit

### 2a. Power

```
    +5 V rail ──────► ESP32 VIN pin
    GND rail ───────► ESP32 GND pin
```

> **Note:** Do not connect both USB and 12 V at the same time unless you
> add a diode on VIN to prevent back-feeding.

### 2b. Development access holes

For each ESP32 pin, solder an extra through-hole pad adjacent to it on
the prototype board, connected to the same trace. Leave these unpopulated
— they provide probe points and temporary connection points during
development.

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

> **Rev 2.1 — opto is IN SERIES with the loop (R2 deleted).** The LED
> sits in the return leg with a current-limit resistor and a series
> diode: **+12 V → R1 → Line A → phone → Line B → R_LIM → D1 → LED →
> GND.** Do **not** wire the LED across Line A↔Line B (the old layout) —
> that carries no loop current and never detects hook or dial.

```
    +12 V ─── [R1 470 Ω 1W] ─── Terminal 1 (Red / Line A)

    Terminal 2 (White / Line B) ─── [R_LIM 2.2 kΩ] ─── D1 anode
    D1 cathode (1N4148, stripe) ─── PC817 pin 1 (Anode)
    PC817 pin 2 (Cathode) ─── GND

    PC817 pin 4 (Collector) ─── +3.3 V (ESP32 3V3 pin)
    PC817 pin 3 (Emitter) ─── GPIO 34
    PC817 pin 3 (Emitter) ─── [R3 10 kΩ] ─── GND
```

**PC817 pinout — check polarity carefully:**

```
    ┌──────────┐
    │  1  ●    │  Anode   ── DOT on package (from D1 cathode / Line B leg)
    │  2       │  Cathode ── to GND
    │  3       │  Emitter ── (to GPIO 34 + R3)
    │  4       │  Collector ── (to +3.3 V)
    └──────────┘
    Pin 1 is marked with a DOT on the IC.
    Current flows: pin 1 (anode) → pin 2 (cathode).
    D1 stripe (cathode) faces the PC817 anode; D1 body points at Line B.
```

> **Polarity is critical.** If the optocoupler or D1 is backwards, hook
> detection will not work. The dot on the package marks pin 1.
>
> **⚠️ Never apply the 48 V bell supply without R_LIM fitted.** Line B
> carries 48 V during ringing, and R_LIM is the only current limit in
> this leg (R1 is on the Line A side). On the bench, omitting R_LIM let
> the 48 V ring breach the opto and cook both the PC817 **and the
> ESP32**. R_LIM = 2.2 kΩ keeps the ring current to ~22 mA.

---

## Step 5: Wire the Ring Generator (L293D H-Bridge)

```
    L293D pin 1  (EN1,2)  ──── GPIO 4
    L293D pin 2  (IN1)    ──── GPIO 16
    L293D pin 7  (IN2)    ──── GPIO 17
    L293D pin 8  (VS)     ──── +48 V (from 48 V DC adapter)
    L293D pin 16 (VSS)    ──── +5 V
    L293D pin 3  (OUT1)   ──── Terminal 3 (Blue / Bell wire)
    L293D pin 6  (OUT2)   ──── Terminal 2 (White / Line B)
    L293D pins 4, 5, 9, 12, 13 ──── GND
```

> **All five GND pins must be connected.** They also act as the heat
> sink — connect them to a copper pour if possible.

**L293D pinout (DIP-16) — notch/dot at pin 1 end:**

```
          ┌────────┐
   EN1,2  │ 1●  16 │  VSS (+5V)
     IN1  │ 2   15 │  EN3,4 (n/c)
    OUT1  │ 3   14 │  IN4 (n/c)
     GND  │ 4   13 │  GND
     GND  │ 5   12 │  GND
    OUT2  │ 6   11 │  OUT4 (n/c)
     IN2  │ 7   10 │  IN3 (n/c)
 VS(48V)  │ 8    9 │  GND
          └────────┘
```

Place a **100 nF ceramic capacitor (C1)** between pin 16 (VSS) and the
nearest GND pin, as close to the IC as possible.

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

    Audio transformer SECONDARY pin 1 ──[ Cc ]── Terminal 1 (Red / Line A)
    Audio transformer SECONDARY pin 2 ────────── Terminal 2 (White / Line B)
```

> **⚠️ Cc (DC-block coupling cap) is MANDATORY.** The transformer
> secondary is only ~70–120 Ω at DC, so wiring it directly across
> Terminal 1 ↔ Terminal 2 shorts the line at DC — this swamps the hook
> loop (detection fails) and saturates the core (distortion). Fit **Cc
> in series with one secondary leg**: 10 µF non-polar ≥63 V, **or** two
> 10 µF 63 V electrolytics back-to-back (− to −) ≈ 5 µF non-polar. This
> supersedes the old "10 Ω on the primary" note.

---

## Step 7: Wire the SD Card Module

```
    SD module pin order (left to right, facing pins):
    CS  SCK  MOSI  MISO  VCC  GND

    GPIO 5  ──── CS
    GPIO 18 ──── SCK (CLK)
    GPIO 23 ──── MOSI (DI)
    GPIO 19 ──── MISO (DO)
    +3.3 V  ──── VCC
    GND     ──── GND
```

> Most SD card modules have an on-board 3.3 V regulator, so you can
> safely connect VCC to 3.3 V or 5 V depending on the module. Check
> your module's documentation.

Place a **100 nF ceramic capacitor (C2)** between the ESP32 VIN pin
and GND, close to the ESP32 board.

---

## Step 8: Wire the Control Panel

### 8a. Four buttons

Four momentary push buttons, each wired between the GPIO pin and GND.
No external resistors — the ESP32 enables internal pull-ups in firmware.

```
    GPIO 32 ──── [RING button]   ──── GND
    GPIO 33 ──── [CANCEL button] ──── GND
    GPIO 27 ──── [RESET button]  ──── GND
    GPIO 14 ──── [MODE button]   ──── GND
```

> **Check each button with a multimeter** before soldering: when pressed,
> continuity between the two pins should read < 1 Ω. When released,
> open circuit.

### 8b. Panel lamp

For a 3 V LED with 100 Ω series resistor:

```
    GPIO 13 ──── [100 Ω] ──── LED anode (+)
                               LED cathode (-) ──── GND
```

> The longer LED leg is the anode (+). The flat side of the LED
> housing is the cathode (-).

For a 5-6 V filament lamp with transistor driver:

```
    GPIO 13 ──── [1 kΩ] ──── 2N2222 Base
                              2N2222 Emitter ──── GND
                              2N2222 Collector ── Lamp (-) terminal
                              Lamp (+) terminal ── +5 V
```

---

## Step 9: Wire the Coin Box Pull-Ups and Header

### 9a. Pull-up resistors (required even without daughter board)

```
    +3.3 V ──┬──────────┬──────────┐
             │          │          │
           [R4]       [R5]       [R6]
           10kΩ       10kΩ       10kΩ
             │          │          │
    GPIO 36 ─┘  GPIO 39 ┘  GPIO 35 ┘
```

> **These pull-ups are essential.** GPIO 36, 39, 35 have no internal
> pull-up. Without R4/R5/R6, the firmware falsely detects a coin box
> and blocks normal phone operation.

### 9b. 6-pin daughter board header

```
    Header Pin 1 ──── GPIO 36 (+ R4 pull-up to 3.3 V)
    Header Pin 2 ──── GPIO 39 (+ R5 pull-up to 3.3 V)
    Header Pin 3 ──── GPIO 35 (+ R6 pull-up to 3.3 V)
    Header Pin 4 ──── +3.3 V
    Header Pin 5 ──── GND
    Header Pin 6 ──── +5 V
```

Mount this header at the board edge for easy daughter board connection.

---

## Step 10: Prepare the SD Card

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

See `docs/audio-files.md` for the complete list of required and optional
audio files.

---

## Testing Procedure

> **Quick start:** once the board boots, send `T` on serial (or the **self-test**
> button on the web Terminal tab) to run the whole checklist in one shot — SD,
> line-sense reading, bell strike, 1 kHz earpiece tone, panel buttons, coin box
> and heap. The individual tests below explain each item and how to fix failures.

### Commissioning commands (serial / web Terminal)

| Command | Serial | Web Terminal | Purpose |
|---------|--------|--------------|---------|
| Self-test | `T` | `selftest` | One-shot bring-up checklist (PASS/FAIL/WARN). |
| Calibrate line | `K` | `calibrate on` then `calibrate off` | Capture on-hook + off-hook ADC levels and auto-set the hook thresholds, saved to `/system/settings.json`. Run this after any change to R_LIM or the opto leg. |
| Dial echo | `E` | `dialecho` | Toggle rotary self-confirm — each dialled digit is blinked on the panel lamp (0 = 10 blinks) so dialling can be verified with no laptop. |
| Audio probe | `Q` | `probe` | Play a 1 kHz tone and report the peak/RMS/crest of the samples fed to I2S (digital side only). |
| Line debug | `N` | — (serial only) | Stream raw `line=` values and dial `BREAK`/`make` pulse timing. |

### Test 1: Power (no phone connected)

1. Connect the **12 V adapter only** first. Do not connect 48 V yet.
2. Verify with a multimeter:
   - Buck converter output = **5.0 V** (±0.2 V)
3. Confirm the ESP32 boots — serial monitor (115200 baud) should show:
   ```
   K6 GPO Exhibit — ESP32 Phone Interface
   [audio] SD card ready
   [phone] controller ready
   ```
4. Verify the boot lamp indication: LED turns on, blinks, then bell
   strikes once when boot completes.
5. Now connect the **48 V adapter**. Verify 48 V at the L293D pin 8 with
   a multimeter.

### Test 2: Coin Box Detection

1. With **no daughter board connected**, check serial output at boot:
   ```
   [coin] no coin box detected — feature disabled
   ```
2. If it says "DETECTED" instead, check R4/R5/R6 pull-up wiring.

### Test 3: SD Card

1. Send `S` via serial. Check output shows `sd=OK`.
2. If `sd=FAIL`, check SPI wiring and card format.

### Test 4: Buttons

1. Send `S` via serial to check current state.
2. Press each button and verify serial output:
   - RING → `[app] RING pressed` (bell should ring if phone is on-hook)
   - CANCEL → `[app] CANCEL pressed` (stops ringing if active)
   - RESET → ESP32 reboots
   - MODE → `[app] MODE toggled` (switches auto/manual)
3. If a button doesn't respond:
   - Check continuity from GPIO pin to button terminal
   - Check continuity from other button terminal to GND
   - Verify button is not shorted (stuck low)

### Test 5: Hook Detection

1. Connect the phone's 3-core cord to the terminal block.
2. With handset **on the cradle**, send `S` — should show `hook=ON_HOOK`.
3. **Lift the handset** — serial should print:
   ```
   [app] hook: OFF_HOOK
   [phone] → DIAL_TONE
   ```
   You should hear the dial tone in the earpiece.
4. If on/off-hook aren't cleanly detected, run the calibration wizard: send
   `K` (or `calibrate on` / `calibrate off` in the web Terminal), which captures
   both ADC levels and sets the thresholds automatically — no need to edit
   `config.h`.

### Test 6: Rotary Dialling

1. With handset lifted, dial digit **5**.
2. Serial should print `[phone] digit: 5`. (Send `E` first to also blink each
   digit on the panel lamp for a hands-off check.)
3. Wait 3 seconds — if `/numbers/5.mp3` exists it plays, otherwise
   you'll hear "number not recognised".

### Test 7: Bell Ringing

1. Replace the handset (on-hook).
2. Send `R` via serial **or** press the RING button.
3. The bell should ring with the UK cadence (ring-ring, pause).
4. Lift the handset — ringing stops, a random history track plays.

### Test 8: Panel Lamp

1. Check the lamp turns on solid in AUTO mode.
2. Press MODE → lamp should turn off (MANUAL mode).
3. Press MODE again → lamp turns back on (AUTO mode).

---

## Troubleshooting

| Symptom | What to check |
|---------|--------------|
| ESP32 doesn't boot | Check 5 V rail with multimeter. Ensure VIN (not 3.3 V) is connected to +5 V. |
| No hook detection | Confirm opto is wired IN SERIES (Line B → R_LIM → D1 → LED → GND), not across the line. Confirm Cc is fitted (a directly-connected transformer secondary DC-shorts the line and holds the opto saturated). Check opto + D1 orientation (dot = pin 1 = anode). Use the `N` serial command to watch the raw `line=` value swing on/off hook. |
| Dial pulses not counted | Enable the `N` serial command and dial — you should see `BREAK`/`make` pairs with `break_len` of ~20–120 ms. If the raw value doesn't drop below the off threshold on a break, run the `K` calibration wizard (or lower R_LIM). Calibration replaces the old need to hand-edit `LINE_THRESHOLD_ON/OFF` in `config.h`. |
| Bell doesn't ring | Verify 48 V at L293D pin 8. Check all 5 GND pins on L293D. GPO 232 needs an external bellset. |
| Bell too quiet | 48 V is lower than the GPO spec of 60-80 V. Consider a higher voltage adapter (up to 60 V — check L293D absolute max). |
| No audio / no sound | Check MAX98357A wiring (BCLK, LRC, DIN). Try `V9` serial command for max volume. Check transformer orientation. |
| Audio distorted | First run `Q` (audio probe) — a clean digital feed reads crest ≈ 1.41; a much lower ratio means the samples are already clipped before I2S (lower the line level / volume). If the digital side is clean, confirm Cc (DC-block cap) is fitted in series with the transformer secondary — without it, DC bias current saturates the core. If still distorted with Cc, listen at the amp output (MAX98357A L+/L−) with a small speaker: clean there ⇒ transformer at fault; distorted there ⇒ MAX98357A module at fault. |
| SD card not detected | Check SPI wiring (CS, MOSI, MISO, SCK). Ensure card is FAT32 formatted. Try a different card. |
| MP3 doesn't play | Check file paths match exactly (`/system/`, `/history/`, `/numbers/`). Ensure valid MP3 encoding. |
| Buttons don't work | Check wiring to GND. Verify correct GPIO numbers. Use multimeter to confirm button makes contact. |
| False A+B coin box detected | Check R4/R5/R6 (10 kΩ pull-ups to 3.3 V) on GPIO 36/39/35. All three must be present. |
| ESP32 resets during ring | Check GND tie point between 12 V and 48 V supplies. Add 470 µF capacitor on 5 V rail. |
| MODE button triggers at boot | Normal — GPIO 14 has internal pull-down during boot. Firmware ignores buttons until setup() completes. |
| Lamp doesn't light | Check LED polarity (long leg = anode = +). Check GPIO 13 goes HIGH with multimeter. |
