# Hardware Schematic — ESP32 GPO Phone Interface (Rev 2)

> **Revision 2** — redesigned for two separate DC supplies (12 V + 48 V),
> corrected pull-ups on input-only GPIOs, four control buttons,
> development access holes on every ESP32 pin, and a 6-pin daughter
> board header.

> All components are housed in a **separate enclosure** connected to the
> phone via an extended 3-core lead.

---

## Revision Notes — What Changed from Rev 1

| Issue (Rev 1) | Root Cause | Fix (Rev 2) |
|---------------|-----------|-------------|
| False A+B coin box detection when no daughter board attached | GPIO 36/39/35 are input-only with **no internal pull-up**. They float randomly — a LOW reading during the 2 s boot window triggers false detection. | Added **external 10 kΩ pull-up resistors** (R4, R5, R6) to 3.3 V on each coin box GPIO. Pins now read HIGH reliably when nothing is connected. |
| Buttons not responding (except CANCEL intermittently) | Likely wiring/positioning error on the PCB. GPIO 14 (MODE button) also has an internal pull-down during ESP32 boot, which may fight the signal. The original docs only listed 3 buttons, but firmware uses 4. | All 4 buttons clearly positioned with their own PCB zone. GPIO 14 pull-down is only active during boot — the internal pull-up is set in firmware after boot, so this is fine for a button. Ensure button wiring is correct per the new layout. |
| Polarity issues | Component orientation not clearly marked on the stripboard. | New layout includes clear polarity markers for the optocoupler, L293D, and all electrolytic capacitors. |
| Single 12 V supply for everything | XL6009 boost converter draws heavily during ringing, causing voltage dips on the 5 V rail → ESP32 brownout. | **Two separate supplies**: 12 V 1 A for logic (5 V buck), 48 V 1 A for bell (direct to L293D VS). No shared current path. |

---

## 1. Power Supply

Two **separate** DC adapters power the system. This eliminates voltage
dips during bell ringing that caused ESP32 resets in Rev 1.

```
    ┌─────────────────────┐         ┌─────────────────────┐
    │  12 V DC  1 A       │         │  48 V DC  1 A       │
    │  (barrel jack)      │         │  (barrel jack)      │
    │                     │         │                     │
    │  Powers:            │         │  Powers:            │
    │  • Buck converter   │         │  • L293D H-bridge   │
    │    (12 V → 5 V)     │         │    pin 8 (VS)       │
    │  • Line sense       │         │    for bell ringing  │
    │    circuit (R1)     │         │                     │
    └─────────┬───────────┘         └─────────┬───────────┘
              │                               │
     ┌────────┴────────┐                      │
     │  Buck Converter  │                      │
     │  LM2596 module   │                      │
     │  12 V → 5 V      │                      │
     │  (adjust trimpot) │                     │
     └────────┬─────────┘                      │
              │                                │
         +5 V rail                        +48 V rail
         Powers:                          (bell only)
         • ESP32 VIN
         • MAX98357A VIN
         • SD card module
         • L293D pin 16 (VSS)
```

### Power Bus Wiring

The board has **two bus rails** running the full length:

| Bus | Voltage | Source | Connects to |
|-----|---------|--------|-------------|
| **5 V bus** | +5 V / GND | LM2596 output | ESP32 VIN, MAX98357A VIN, SD card VCC, L293D pin 16 |
| **48 V bus** | +48 V / GND | 48 V adapter | L293D pin 8 (VS) only |

> **IMPORTANT:** The GND rails of both supplies must be **connected
> together** at a single point on the board. This provides a common
> ground reference. Connect them near the L293D GND pins.

### Components

| Part | Example | Purpose |
|------|---------|---------|
| 12 V DC adapter | Any 12 V / 1 A barrel-jack adapter | Logic power |
| 48 V DC adapter | Any 48 V / 1 A barrel-jack adapter | Bell power |
| Buck converter module | LM2596 adjustable (set to 5 V) | 12 V → 5 V for logic |

> **Setup:** Before connecting anything else, power the buck converter
> from 12 V and adjust its trimpot until the output reads **5.0 V** on a
> multimeter.

---

## 2. Hook / Dial Pulse Detection

This circuit detects whether the handset is on the cradle (on-hook) or
lifted (off-hook), and senses the rotary dial pulses — all on a single
GPIO pin.

> **Rev 2.1 correction — the optocoupler must be *in series* with the
> loop, not bridged across it.** The original Rev 2 layout wired the
> PC817 LED across the line (Line A via R2 → LED → Line B), with Line B
> having no return to ground. That topology can carry **no loop current
> in any hook state**, so hook and dial detection never worked. The LED
> is now in series in the return leg: **+12 V → R1 → Line A → phone →
> Line B → R_LIM → D1 → LED → GND.** R2 is deleted. This was verified on
> the bench (hook + dial both decode correctly). See the warning below
> before ever applying the 48 V bell supply.

```
    +12 V ────────┐
                  │
                 [R1]  470 Ω  1W
                  │
                  └──────────── Terminal 1 (RED wire = Line A)

                  ═══ phone loop ═══  (open on-hook, ~360 Ω off-hook)

    Terminal 2 ───┐
    (WHITE wire   │
     = Line B)   [R_LIM]  2.2 kΩ        ← current limit (MANDATORY, see warning)
                  │
                  ▼  D1  (1N4148, anode → Line B side)
                  │
             ┌────┴────┐
             │  PC817   │
             │ OPTO-    │
             │ COUPLER  │
             │  Anode  ┌┤
             │    (p1) ││── Collector (p4) ─── +3.3 V
             │  LED    └┤
             │ Cathode   │
             │  (p2)     Emitter (p3) ──┬── GPIO 34 (ESP32)
             └────┬────┘                │
                  │                    [R3]  10 kΩ
                 GND                     │
                                        GND
```

**PC817 pinout (DIP-4) — polarity markers:**

```
    ┌──────────┐
    │  Pin 1   │  Anode  ── dot/band on package (from D1 cathode / Line B leg)
    │  Pin 2   │  Cathode ── to GND
    │  Pin 3   │  Emitter ── (to GPIO 34 + R3)
    │  Pin 4   │  Collector ── (to +3.3 V)
    └──────────┘
    Pin 1 is marked with a dot on the IC package.
    The LED inside goes from pin 1 (anode) to pin 2 (cathode).
```

**How it works:**

| Phone state | What happens | GPIO 34 reads |
|------------|-------------|---------------|
| **On-hook** (handset down) | Loop open — no current, LED off | LOW (~0 V) |
| **Off-hook** (handset lifted) | Loop closed — ~4-13 mA flows through R1 → phone → R_LIM → D1 → LED → GND | HIGH (~3.3 V) |
| **Dial pulse** (rotary dial break) | Loop briefly interrupted → LED off | LOW pulse (20-120 ms) |

The firmware counts these LOW pulses to decode the dialled digit
(1 pulse = digit 1, 10 pulses = digit 0).

> ### ⚠️ SAFETY: R_LIM is mandatory before applying 48 V
>
> Line B carries the **48 V ring voltage** during ringing. The opto LED
> leg (Line B → D1 → LED → GND) has **no other current limit** — R1 is
> on the Line A side and is *not* in this path. Without **R_LIM
> (1.5–2.2 kΩ)** in the Line B leg, the 48 V ring drives destructive
> current through the LED.
>
> **This is not theoretical:** on the bench, applying 48 V with R_LIM
> omitted breached the PC817's isolation and back-fed the ESP32, cooking
> both the optocoupler *and* the ESP32 module (it powered up but ran
> hot). **Never apply 48 V unless R_LIM is fitted.**
>
> - Ring peak current with R_LIM = 2.2 kΩ: 48 V / 2.2 kΩ ≈ **22 mA**
>   (safe for the PC817).
> - Off-hook detect current: (15 V − ~1.9 V) / (470 + 360 + 2200) ≈
>   **4–5 mA** (enough to trigger; drop R_LIM to 1.5 kΩ if marginal).
> - **D1 (series diode)** blocks the reverse half of the ring, so a
>   separate anti-parallel clamp diode is *not* required.

---

## 3. Ring Generator

Makes the bell ring using a 48 V square-wave AC signal at 25 Hz,
switched by an L293D H-bridge IC.

```
    +48 V (from 48 V DC adapter) ── L293D pin 8 (VS)

    +5 V ──────────────────────────── L293D pin 16 (VSS / logic supply)

    ESP32 GPIO 4  ─────────────────── L293D pin 1 (EN1,2 — enable)
    ESP32 GPIO 16 ─────────────────── L293D pin 2 (IN1)
    ESP32 GPIO 17 ─────────────────── L293D pin 7 (IN2)

    L293D pin 3 (OUT1) ──────────── Terminal 3 (BLUE wire = Bell)
    L293D pin 6 (OUT2) ──────────── Terminal 2 (WHITE wire = Line B)

    L293D pins 4, 5, 12, 13 ─────── GND (common ground bus)
    L293D pin 9 ──────────────────── GND
```

**How it works:**

The firmware alternates IN1/IN2 at 25 Hz while EN is HIGH (with PWM
for volume control), producing a square wave that swings between +48 V
and -48 V across the bell coil.  The GPO 332's internal 2 µF capacitor
passes this AC to the bell mechanism.

> **Note:** Rev 1 used a boost converter to generate 50 V from 12 V.
> Rev 2 uses a dedicated 48 V DC supply, eliminating the boost converter
> entirely. 48 V is sufficient to ring the bell — the GPO spec is
> 60-80 V AC, but the capacitive impedance at 25 Hz means 48 V
> produces adequate striking force.

**L293D pinout (DIP-16) — notch at top:**

```
                 ┌───┐
    EN1,2  1 ────┤   ├──── 16  VSS (+5 V logic)
      IN1  2 ────┤   ├──── 15  EN3,4 (n/c)
     OUT1  3 ────┤   ├──── 14  IN4 (n/c)
      GND  4 ────┤   ├──── 13  GND
      GND  5 ────┤   ├──── 12  GND
     OUT2  6 ────┤   ├──── 11  OUT4 (n/c)
      IN2  7 ────┤   ├──── 10  IN3 (n/c)
  VS(+48V) 8 ────┤   ├──── 9   GND
                 └───┘
    Notch/dot at pin 1 end. Mount with notch facing away from
    the 48 V bus to keep high-voltage traces on one side.
```

---

## 4. Audio Output (I2S DAC → Phone Line)

Audio plays from the SD card through an I2S DAC module, then into the
phone line via a coupling transformer.

```
    ESP32                     MAX98357A              Audio Transformer
    GPIO 26 ── BCLK ────────► BCLK                 (600 Ω : 600 Ω, 1:1)
    GPIO 25 ── LRCLK ───────► LRC
    GPIO 22 ── DIN ──────────► DIN        L+ ──────► Primary pin 1 ──┐
    5 V ─────── VIN ─────────► VIN                                    │(transformer)
    GND ─────── GND ─────────► GND        L- ──────► Primary pin 2 ──┘
                                                           │
                                        Secondary pin 1 ──[ Cc ]── Terminal 1 (Line A)
                                        Secondary pin 2 ────────── Terminal 2 (Line B)
```

**How it works:**

The MAX98357A decodes I2S audio into an analogue signal on L+/L-.
The coupling transformer isolates this from the phone line's DC bias
and matches impedance.  Audio passes through the phone's internal
induction coil to the earpiece.

> ### ⚠️ Cc — DC-blocking coupling capacitor is MANDATORY
>
> The transformer secondary winding is only **~70–120 Ω at DC**, so if
> it connects directly across Terminal 1 ↔ Terminal 2 it forms a **DC
> short across the line**. That short:
> 1. **Swamps the hook loop** — the phone's 360 Ω (off-hook) vs. open
>    (on-hook) swing is invisible next to a fixed ~120 Ω, so the
>    optocoupler stays saturated and hook/dial detection fails; and
> 2. **Saturates the transformer core** with the DC bias current now
>    flowing (+12 V → R1 → Line A → secondary → Line B → LED → GND),
>    adding gross distortion.
>
> Fit **Cc in series with one secondary leg** (Line A leg shown):
> - **10 µF non-polar / bipolar, ≥63 V** (film or bipolar electrolytic), **or**
> - **two 10 µF 63 V electrolytics in series, back-to-back** (join the
>   two like terminals, − to −) ≈ **5 µF non-polar, 63 V** — perfectly
>   adequate for telephone-band voice (~106 Ω at 300 Hz).
>
> Cc passes voice but blocks DC, so the line is no longer shorted:
> hook + dial work, the core no longer saturates, and 25 Hz ring bleed
> into the earpiece is attenuated. The old "10 Ω in series with the
> primary" note is superseded by this.

---

## 5. SD Card (SPI)

Standard micro-SD card breakout module, using the ESP32's default VSPI bus.

```
    ESP32                SD Card Module
    GPIO 5  ── CS ──────► CS
    GPIO 23 ── MOSI ────► MOSI (DI)
    GPIO 19 ── MISO ◄───  MISO (DO)
    GPIO 18 ── SCK ─────► SCK (CLK)
    3.3 V ───── VCC ────► VCC
    GND ─────── GND ────► GND
```

---

## 6. Control Panel — Four Buttons

Four momentary push buttons on the operator's control box.
No external resistors needed — the ESP32's internal pull-ups are enabled
in firmware. Each button connects its GPIO pin to GND when pressed.

```
    ESP32 GPIO 32 ──── [RING button]   ──── GND
    ESP32 GPIO 33 ──── [CANCEL button] ──── GND
    ESP32 GPIO 27 ──── [RESET button]  ──── GND
    ESP32 GPIO 14 ──── [MODE button]   ──── GND
```

| Button | What it does |
|--------|-------------|
| RING | Manually triggers the phone bell |
| CANCEL | Stops ringing or current audio playback |
| RESET | Reboots the ESP32 |
| MODE | Toggles between Automatic and Manual ringing mode |

> **Note on GPIO 14:** This pin has an internal pull-down during the
> ESP32 boot sequence (it is used for JTAG). The MODE button may read
> as "pressed" momentarily during the first ~100 ms of boot. The
> firmware ignores button inputs until after `setup()` completes, so
> this has no effect in practice. After boot, `INPUT_PULLUP` is
> configured and the pin behaves normally.

---

## 7. Auto-Mode Panel Lamp

A small panel indicator lamp (3-6 V filament or LED with resistor)
driven by a GPIO via a simple transistor switch if needed, or directly
for an LED.

```
    ESP32 GPIO 13 ──── [lamp +] ──── [lamp -] ──── GND
```

For a 3 V LED: add a **100 Ω resistor** in series.
For a 5-6 V filament lamp: use a **2N2222 NPN transistor** as a switch:

```
    GPIO 13 ── [1 kΩ] ── Base
                         Collector ── [lamp +]
                         Emitter ──── GND
                         [lamp -] ──── +5 V
```

| Lamp state | Meaning |
|-----------|---------|
| Steady ON | Automatic ringing mode active |
| OFF | Manual mode |
| Flashing (500 ms) | Inactivity warning — no visitors for configured period |
| Fast blink (100 ms) | System booting |

---

## 8. A+B Coin Box Daughter Board Header

A 6-pin header connects the optional A+B coin box daughter board.
When no daughter board is installed, the **external pull-up resistors**
(R4, R5, R6) hold the three input pins HIGH, and the firmware
correctly disables the coin box feature.

### Mainboard wiring (Rev 2 — with pull-ups)

```
    +3.3 V ──┬──────────┬──────────┐
             │          │          │
            [R4]       [R5]       [R6]    ← 10 kΩ each
            10kΩ       10kΩ       10kΩ
             │          │          │
    GPIO 36 ─┤  GPIO 39 ┤  GPIO 35 ┤
             │          │          │
             │          │          │
             ▼          ▼          ▼
          Header      Header     Header
          Pin 1       Pin 2      Pin 3

    Header Pin 4 ──── +3.3 V (power for daughter board optocouplers)
    Header Pin 5 ──── GND
    Header Pin 6 ──── +5 V (optional, for daughter board LED drive)
```

### 6-pin header pinout

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1 | COIN_SENSE (GPIO 36) | Input | Coin weight switch |
| 2 | BTN_A (GPIO 39) | Input | Button A (collect coins) |
| 3 | BTN_B (GPIO 35) | Input | Button B (refund coins) |
| 4 | +3.3 V | Power | Supplies daughter board |
| 5 | GND | Power | Common ground |
| 6 | +5 V | Power | Optional — for optocoupler LED drive |

### Why external pull-ups are essential

GPIO 36, 39, and 35 are **input-only** pins on the ESP32. Unlike
regular GPIOs, they do **not** support the internal pull-up resistor
(`INPUT_PULLUP` has no effect on these pins). Without external pull-ups:

- The pins float at an indeterminate voltage
- Random LOW readings during the 2-second boot detection window
  cause the firmware to falsely detect a daughter board
- Once falsely detected, the coin box logic blocks normal call flow
  (waits for Button A press that never comes)

The 10 kΩ pull-ups hold the pins at 3.3 V (HIGH). When the daughter
board is connected, its optocoupler outputs pull the pins LOW through
a lower impedance (~1 kΩ), overriding the pull-ups cleanly.

---

## 9. Protection Diodes

The LINE_B net is shared between the L293D output (48 V during ringing),
the opto LED leg (R_LIM → D1 → PC817 anode), and the transformer
secondary (via Cc). Without protection, the ring voltage damages the
optocoupler and can back-feed into the DAC.

### R_LIM + D1 — opto LED current limit and reverse block (Rev 2.1)

```
    Terminal 2 (LINE_B) ──[R_LIM 2.2 kΩ]──►|── PC817 pin 1 (Anode)
                                          D1
                                       (1N4148,
                                    anode→Line B side)

    PC817 pin 2 (Cathode) ──── GND
```

**R_LIM (1.5–2.2 kΩ)** sits in series in the opto LED leg and limits
both the off-hook loop current (~4–5 mA) and, crucially, the **48 V ring
current (~22 mA at 2.2 kΩ)**. It is **mandatory** — R1 is on the Line A
side and does not protect this leg (see the Section 2 safety warning:
omitting R_LIM cooked the opto *and* the ESP32 on the bench).

**D1 in series** (anode toward Line B, cathode toward the LED anode)
passes the off-hook / dial loop current but **blocks the reverse half of
the 48 V ring**, so the LED is never reverse-biased. Because D1 is in
series, the old anti-parallel clamp diode is no longer used.

### D2–D5 — DAC overvoltage clamps

```
    +5V ──────────┬──────────────┐
                  │              │
              D2 cathode    D4 cathode
              D2 anode      D4 anode
                  │              │
              DAC_LP          DAC_LN
              (J_SPK pin 1)   (J_SPK pin 2)
                  │              │
              D3 cathode    D5 cathode
              D3 anode      D4 anode
                  │              │
    GND ──────────┴──────────────┘
```

These four diodes clamp the transformer primary voltage to between
−0.7 V and +5.7 V. During normal audio playback the DAC output stays
within 0–5 V, so the diodes do not conduct. During ringing, any
coupled voltage that exceeds these limits is safely shunted to the
+5 V or GND rail.

---

## Master Interconnect Table

Every wire in the system, listed by destination:

| From | To | Wire/Note |
|------|----|-----------|
| **12 V adapter +** | Buck converter IN+ | Logic power in |
| **12 V adapter +** | R1 (470 Ω) top end | Line sense supply |
| **12 V adapter -** | Common GND bus | Ground reference |
| **48 V adapter +** | L293D pin 8 (VS) | Bell power |
| **48 V adapter -** | Common GND bus | Ground reference (join to 12 V GND) |
| Buck converter OUT+ (5 V) | ESP32 VIN pin | Logic power |
| Buck converter OUT+ (5 V) | MAX98357A VIN | DAC power |
| Buck converter OUT+ (5 V) | L293D pin 16 (VSS) | H-bridge logic |
| Buck converter OUT+ (5 V) | SD card module VCC | SD card power |
| Buck converter OUT+ (5 V) | Header pin 6 | Daughter board (optional) |
| ESP32 3.3 V | PC817 Collector (pin 4) | Optocoupler pull-up |
| ESP32 3.3 V | R4, R5, R6 (top) | Coin box GPIO pull-ups |
| ESP32 3.3 V | Header pin 4 | Daughter board power |
| ESP32 3.3 V | SD card VCC (if 3.3 V module) | Some modules need 3.3 V |
| R1 (470 Ω) bottom | Terminal 1 (Line A) | Line A feed (R2 deleted in Rev 2.1) |
| Terminal 2 (Line B) | R_LIM (2.2 kΩ) top | Opto LED leg current limit |
| R_LIM (2.2 kΩ) bottom | D1 anode | Series limit → reverse block |
| D1 cathode (1N4148) | PC817 Anode (pin 1) | Loop drive, reverse-blocked |
| PC817 Cathode (pin 2) | GND | Loop return to ground |
| PC817 Emitter (pin 3) | GPIO 34 + R3 to GND | Hook/dial sense |
| L293D OUT1 (pin 3) | Terminal 3 (Blue/Bell) | Ring signal |
| L293D OUT2 (pin 6) | Terminal 2 (White/Line B) | Ring return |
| L293D pins 4,5,9,12,13 | GND | All GND pins connected |
| MAX98357A L+ | Transformer primary 1 | Audio out |
| MAX98357A L- | Transformer primary 2 | Audio out |
| Transformer secondary 1 | Cc → Terminal 1 (Red/Line A) | Audio to phone via DC-block cap Cc |
| Transformer secondary 2 | Terminal 2 (White/Line B) | Audio to phone |
| GPIO 32 | RING button → GND | Control panel |
| GPIO 33 | CANCEL button → GND | Control panel |
| GPIO 27 | RESET button → GND | Control panel |
| GPIO 14 | MODE button → GND | Control panel |
| GPIO 13 | Panel lamp → GND | Status indicator |
| GPIO 36 | Header pin 1 (+ R4 to 3.3 V) | Coin sense |
| GPIO 39 | Header pin 2 (+ R5 to 3.3 V) | Button A |
| GPIO 35 | Header pin 3 (+ R6 to 3.3 V) | Button B |
| D1 anode | R_LIM bottom (Line B leg) | Series diode (blocks reverse ring) |
| D1 cathode | PC817 Anode (pin 1) | Series diode (blocks reverse ring) |
| D2 anode | DAC_LP (J_SPK pin 1 / T1 pri 1) | Positive clamp on L+ |
| D2 cathode | +5V rail | Positive clamp on L+ |
| D3 anode | GND | Negative clamp on L+ |
| D3 cathode | DAC_LP (J_SPK pin 1 / T1 pri 1) | Negative clamp on L+ |
| D4 anode | DAC_LN (J_SPK pin 2 / T1 pri 2) | Positive clamp on L- |
| D4 cathode | +5V rail | Positive clamp on L- |
| D5 anode | GND | Negative clamp on L- |
| D5 cathode | DAC_LN (J_SPK pin 2 / T1 pri 2) | Negative clamp on L- |
| Terminal 1 | Red wire (Line A) | Phone cord |
| Terminal 2 | White wire (Line B) | Phone cord |
| Terminal 3 | Blue wire (Bell) | Phone cord |

---

## PCB Layout Guide (Rev 2)

### Board zones

Organise the prototype board into clearly separated zones to avoid
interference and make assembly/debugging easier:

```
    ┌─────────────────────────────────────────────────────────────┐
    │  ZONE A: POWER                                              │
    │  ┌──────────────────────────┐  ┌────────────────┐           │
    │  │  12 V barrel jack        │  │  48 V barrel   │           │
    │  │  Buck converter module   │  │  jack           │           │
    │  │  5 V / GND bus start     │  │  48 V bus       │           │
    │  └──────────────────────────┘  └────────────────┘           │
    │  GND tie point (12 V GND ── 48 V GND) ●                    │
    ├─────────────────────────────────────────────────────────────┤
    │  ZONE B: ESP32 + DEV ACCESS                                 │
    │  ┌──────────────────────────────────────────────┐           │
    │  │  ESP32 DevKit V1 (30-pin, 2×15 headers)      │           │
    │  │  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○              │           │
    │  │  █ █ █ █ █ █ █ █ █ █ █ █ █ █ █              │  ← ESP32  │
    │  │  █ █ █ █ █ █ █ █ █ █ █ █ █ █ █              │  module   │
    │  │  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○              │           │
    │  │           USB port → board edge               │           │
    │  └──────────────────────────────────────────────┘           │
    │  ○ = dev access hole (1 per pin, adjacent to each ESP32 pin)│
    ├─────────────────────────────────────────────────────────────┤
    │  ZONE C: PERIPHERALS                   │  HIGH VOLTAGE     │
    │  ┌──────────────────────────┐          │  ┌──────────────┐ │
    │  │  MAX98357A DAC module    │          │  │  L293D        │ │
    │  │  SD card module          │          │  │  (DIP-16)     │ │
    │  │  Audio transformer       │          │  │  48 V traces  │ │
    │  │  PC817 optocoupler       │          │  │  Bell output  │ │
    │  │  R1, R2, R3              │          │  └──────────────┘ │
    │  └──────────────────────────┘          │                   │
    ├─────────────────────────────────────────────────────────────┤
    │  ZONE D: PROTECTION DIODES                                  │
    │  ┌──────────────┐  ┌──────────────────────────────────────┐ │
    │  │  D1 (1N4007)  │  │  D2, D3 (DAC_LP clamps)             │ │
    │  │  PC817 prot   │  │  D4, D5 (DAC_LN clamps)             │ │
    │  └──────────────┘  └──────────────────────────────────────┘ │
    ├─────────────────────────────────────────────────────────────┤
    │  ZONE E: CONNECTORS                                         │
    │  ┌────────────┐ ┌────────────┐ ┌──────┐ ┌──────────────┐  │
    │  │ 3-way screw │ │ 4× button  │ │ Lamp │ │ 6-pin header │  │
    │  │ terminal    │ │ headers    │ │ conn │ │ (daughter bd) │  │
    │  │ (phone cord)│ │ (RING,     │ │      │ │              │  │
    │  │             │ │  CANCEL,   │ │      │ │              │  │
    │  │             │ │  RESET,    │ │      │ │              │  │
    │  │             │ │  MODE)     │ │      │ │              │  │
    │  └────────────┘ └────────────┘ └──────┘ └──────────────┘  │
    └─────────────────────────────────────────────────────────────┘
```

### Development access holes

Every ESP32 pin gets **one additional through-hole pad** adjacent to it
on the board. These are connected to the same net as the ESP32 pin but
are left unpopulated. Purpose:

- Probe with a multimeter or oscilloscope during development
- Solder temporary jumper wires for testing new features
- Attach clip leads without disturbing the main wiring

Arrange them in a single row running parallel to each side of the ESP32
module, spaced 2.54 mm (standard 0.1" pitch).

### Layout rules

1. **Keep the high-voltage area (L293D / 48 V) physically separated**
   from Zone B (ESP32) and the audio section. Minimum 10 mm clearance
   between 48 V traces and logic traces.
2. **Use a ground plane.** Join analogue and digital grounds at a single
   point near the ESP32.
3. The optocoupler (PC817) provides **galvanic isolation** — do not
   bridge the isolation gap with other traces.
4. Place the **audio transformer close** to the phone terminal block
   to minimise audio trace length.
5. **L293D heat dissipation** — the GND pins (4, 5, 12, 13) also serve
   as the heat sink. Connect them to a generous copper pour on the
   ground plane.
6. **Decoupling capacitors:** Place a 100 nF ceramic capacitor between
   L293D pin 16 (VSS) and GND, close to the IC. Place a 100 nF ceramic
   between the 5 V bus and GND near the ESP32 VIN pin.
7. **Pull-up resistors R4/R5/R6** should be placed close to the 6-pin
   daughter board header, not near the ESP32. This keeps the pull-up
   path short and clean.
8. **D1** should be placed close to the PC817 (Zone D, near Zone C
   peripherals). D2–D5 should be near the DAC speaker terminal (J_SPK).
9. **48 V trace width:** Use 0.75–1.0 mm traces for +48V, BELL, and
   LINE_B nets. Use 0.5 mm for +5V/+12V/+3V3 power rails. Increase
   clearance around 48 V nets to 0.5 mm minimum.

### Recommended board size

**100 mm × 100 mm** double-sided prototype board (standard size,
available from most PCB suppliers). The extra height accommodates
Zone D (protection diodes) with clear separation between all zones.

---

## ESP32 DevKit V1 Pin Map

Complete pin usage table. Pins marked **DEV** are unused by the
firmware and available via the development access holes.

| Pin | GPIO | Function | Direction | Notes |
|-----|------|----------|-----------|-------|
| D2 | 2 | Status LED | Output | On-board LED |
| D4 | 4 | Bell EN (L293D EN1,2) | Output | PWM for bell volume |
| D5 | 5 | SD card CS | Output | SPI chip select |
| D13 | 13 | Panel lamp | Output | Auto-mode indicator |
| D14 | 14 | MODE button | Input | Pull-up in firmware |
| D16 | 16 | Bell IN1 (L293D) | Output | H-bridge phase A |
| D17 | 17 | Bell IN2 (L293D) | Output | H-bridge phase B |
| D18 | 18 | SD card SCK | Output | SPI clock |
| D19 | 19 | SD card MISO | Input | SPI data in |
| D22 | 22 | I2S DIN (MAX98357A) | Output | Audio data |
| D23 | 23 | SD card MOSI | Output | SPI data out |
| D25 | 25 | I2S LRCLK | Output | Audio word select |
| D26 | 26 | I2S BCLK | Output | Audio bit clock |
| D27 | 27 | RESET button | Input | Pull-up in firmware |
| D32 | 32 | RING button | Input | Pull-up in firmware |
| D33 | 33 | CANCEL button | Input | Pull-up in firmware |
| D34 | 34 | Line sense (ADC) | Input | Analogue, input-only |
| D35 | 35 | Coin BTN_B | Input | Input-only, ext pull-up R6 |
| SVN (D36) | 36 | Coin SENSE | Input | Input-only, ext pull-up R4 |
| SVP (D39) | 39 | Coin BTN_A | Input | Input-only, ext pull-up R5 |
| VIN | — | +5 V power in | Power | From buck converter |
| 3V3 | — | +3.3 V out | Power | For optocoupler + pull-ups |
| GND | — | Ground | Power | Common ground bus |
| EN | — | Enable (reset) | — | Leave unconnected (has on-board pull-up) |
| D12 | 12 | DEV | — | Boot-sensitive (avoid pull-high at boot) |
| D15 | 15 | DEV | — | Outputs PWM at boot (may cause brief pulse) |
| D21 | 21 | DEV | — | Available |
| TX0 | 1 | Serial TX | Output | USB serial (debug) |
| RX0 | 3 | Serial RX | Input | USB serial (debug) |

> **Note:** This project uses the **30-pin** ESP32 DevKit V1 (2×15 pin
> headers). GPIO 0 is on the module but not broken out as a header pin —
> it is accessible via the on-board BOOT button. All GPIOs used by the
> firmware are available on the 30-pin version. Position the ESP32 with
> the **USB port facing the board edge** for easy cable access.
