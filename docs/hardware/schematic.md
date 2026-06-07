# Hardware Schematic — ESP32 GPO Phone Interface

> All components are housed in a **separate enclosure** connected to the
> phone via an extended 3-core lead.

---

## 1. Power Supply

A single **12 V DC adapter** (wall-wart, ≥ 1 A) powers the whole system.

```
                    12 V DC Adapter
                    (barrel jack)
                         │
         ┌───────────────┼───────────────┐
         │               │               │
    ┌────┴────┐    ┌─────┴─────┐   ┌─────┴──────┐
    │ Buck    │    │ XL6009    │   │ Direct to  │
    │ Conv.   │    │ Boost     │   │ line sense │
    │ 12V→5V  │    │ 12V→50V   │   │ circuit    │
    │         │    │ (adjust   │   │ via R1     │
    │ OUT: 5V │    │ trimpot)  │   │ (470 Ω)   │
    └────┬────┘    └─────┬─────┘   └────────────┘
         │               │
    Powers:          Powers:
    • ESP32 VIN      • L293D H-bridge
    • MAX98357A        pin 8 (VS)
    • SD card module   for bell ringing
    • L293D logic
      pin 16 (VSS)
```

**Why 12 V?**  A 5 V → 50 V boost (10:1 ratio) is impractical — modules
are hard to source and current-limited.  Starting from 12 V the XL6009
only needs a 4:1 ratio, well within its rated range.  12 V adapters are
cheap and widely available.

### Components

| Part | Example | Purpose | Where to buy |
|------|---------|---------|-------------|
| 12 V DC adapter | Any 12 V / 1 A barrel-jack adapter | Main power | Widely available |
| Buck converter module | LM2596 or MP1584 (12 V → 5 V) | 5 V rail for logic | Amazon / eBay ~£1 |
| Boost converter module | XL6009 adjustable (5-32 V in → 5-50 V out) | 50 V rail for bell | Amazon / eBay ~£1-2 |

> **Setup:** Before connecting anything else, power the XL6009 module from
> 12 V and adjust its trimpot with a small screwdriver until the output
> reads **50 V** on a multimeter.  Similarly, set the buck converter to **5 V**.

---

## 2. Hook / Dial Pulse Detection

This circuit detects whether the handset is on the cradle (on-hook) or
lifted (off-hook), and senses the rotary dial pulses — all on a single
GPIO pin.

```
    +12 V ────────┐
                  │
                 [R1]  470 Ω  1W
                  │
                  ├──────────── Terminal 1 (RED wire = Line A)
                  │
                 [R2]  220 Ω
                  │
             ┌────┴────┐
             │  PC817   │
             │ OPTO-    │
             │ COUPLER  │
             │         ┌┤
             │    LED  │├── Collector ─── +3.3 V
             │         └┤
             │  Anode    │
             └────┬────┘│
                  │     Emitter ──┬── GPIO 34 (ESP32)
                  │               │
    Terminal 2 ───┘              [R3]  10 kΩ
    (WHITE wire                   │
     = Line B)                   GND
```

**How it works:**

| Phone state | What happens | GPIO 34 reads |
|------------|-------------|---------------|
| **On-hook** (handset down) | No current flows — optocoupler LED off | LOW (~0) |
| **Off-hook** (handset lifted) | ~20 mA flows through phone circuit → LED on | HIGH (~3.3 V) |
| **Dial pulse** (rotary dial break) | Current briefly interrupted → LED off | LOW pulse (20-120 ms) |

The firmware counts these LOW pulses to decode the dialled digit
(1 pulse = digit 1, 10 pulses = digit 0).

---

## 3. Ring Generator

Makes the bell ring using a 50 V square-wave AC signal at 25 Hz,
switched by an L293D H-bridge IC.

```
    +50 V (from XL6009 boost) ──── L293D pin 8 (VS)

    +5 V ─────────────────────────── L293D pin 16 (VSS / logic supply)

    ESP32 GPIO 4  ──────────────── L293D pin 1 (EN1,2 — enable)
    ESP32 GPIO 16 ──────────────── L293D pin 2 (IN1)
    ESP32 GPIO 17 ──────────────── L293D pin 7 (IN2)

    L293D pin 3 (OUT1) ─────────── Terminal 3 (BLUE wire = Bell)
    L293D pin 6 (OUT2) ─────────── Terminal 2 (WHITE wire = Line B)

    L293D pins 4, 5, 12, 13 ────── GND
```

**How it works:**

The firmware alternates IN1/IN2 at 25 Hz while EN is HIGH, producing a
square wave that swings between +50 V and -50 V across the bell coil.
The GPO 332's internal 2 µF capacitor passes this AC to the bell mechanism.

**L293D pinout (DIP-16):**

```
                 ┌───┐
    EN1,2  1 ────┤   ├──── 16  VSS (+5V logic)
      IN1  2 ────┤   ├──── 15  EN3,4 (n/c)
     OUT1  3 ────┤   ├──── 14  IN4 (n/c)
      GND  4 ────┤   ├──── 13  GND
      GND  5 ────┤   ├──── 12  GND
     OUT2  6 ────┤   ├──── 11  OUT4 (n/c)
      IN2  7 ────┤   ├──── 10  IN3 (n/c)
    VS(+50V) 8 ──┤   ├──── 9   GND (n/c)
                 └───┘
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
                                               Secondary pin 1 ── Terminal 1 (Line A)
                                               Secondary pin 2 ── Terminal 2 (Line B)
```

**How it works:**

The MAX98357A decodes I2S audio into an analogue signal on L+/L-.
The coupling transformer isolates this from the phone line's DC bias
and matches impedance.  Audio passes through the phone's internal
induction coil to the earpiece.

> A **10 Ω resistor** in series with the transformer primary may be
> needed to limit current and reduce distortion.

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

## 6. Control Panel Buttons

Three momentary push buttons on the operator's control box.
No external resistors needed — the ESP32's internal pull-ups are enabled.

```
    ESP32 GPIO 32 ──── [RING button]   ──── GND
    ESP32 GPIO 33 ──── [CANCEL button] ──── GND
    ESP32 GPIO 27 ──── [RESET button]  ──── GND
```

| Button | What it does |
|--------|-------------|
| RING | Manually triggers the phone bell |
| CANCEL | Stops ringing or current audio playback |
| RESET | Reboots the ESP32 |

---

## 7. Status LED

The on-board LED (GPIO 2) on most ESP32 DevKits:
- **Solid ON** = phone off-hook
- **Blinking** = ringing
- **OFF** = idle

---

## Master Interconnect Table

Every wire in the system, listed by destination:

| From | To | Wire/Note |
|------|----|-----------|
| **12 V adapter +** | Buck converter IN+ | Main power in |
| **12 V adapter +** | XL6009 boost IN+ | Main power in |
| **12 V adapter +** | R1 (470 Ω) top end | Line supply |
| **12 V adapter -** | Common GND bus | Ground reference |
| Buck converter OUT+ (5 V) | ESP32 VIN pin | Logic power |
| Buck converter OUT+ (5 V) | MAX98357A VIN | DAC power |
| Buck converter OUT+ (5 V) | L293D pin 16 (VSS) | H-bridge logic |
| Buck converter OUT+ (5 V) | SD card module VCC | SD card power (3.3 V via module regulator, or use ESP32 3.3 V) |
| XL6009 boost OUT+ (50 V) | L293D pin 8 (VS) | Bell ringing supply |
| R1 (470 Ω) bottom | Terminal 1 / R2 top | Line A feed |
| R2 (220 Ω) bottom | PC817 Anode | Optocoupler drive |
| PC817 Cathode | Terminal 2 (Line B) | Return path |
| PC817 Collector | +3.3 V | Pull-up |
| PC817 Emitter | GPIO 34 + R3 to GND | Hook/dial sense |
| L293D OUT1 (pin 3) | Terminal 3 (Blue/Bell) | Ring signal |
| L293D OUT2 (pin 6) | Terminal 2 (White/Line B) | Ring return |
| MAX98357A L+ | Transformer primary 1 | Audio out |
| MAX98357A L- | Transformer primary 2 | Audio out |
| Transformer secondary 1 | Terminal 1 (Red/Line A) | Audio to phone |
| Transformer secondary 2 | Terminal 2 (White/Line B) | Audio to phone |
| Terminal 1 | Red wire (Line A) | Phone cord |
| Terminal 2 | White wire (Line B) | Phone cord |
| Terminal 3 | Blue wire (Bell) | Phone cord |

---

## PCB Layout Notes

- Keep the 50 V ring generator section physically separated from the
  ESP32 and audio path to minimise noise coupling.
- Use a ground plane.  Join analogue and digital grounds at a single
  point near the ESP32.
- The optocoupler provides galvanic isolation — do not bridge the
  isolation gap with other traces.
- Place the audio transformer close to the phone terminal block.
- Use twisted-pair or shielded cable for the extended lead between
  the phone box and the equipment enclosure.
