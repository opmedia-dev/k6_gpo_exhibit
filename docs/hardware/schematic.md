# Hardware Schematic — ESP32 GPO Phone Interface

## Block Diagram

```
                         ┌──────────────────────────────────────┐
                         │         ESP32 Interface Board        │
                         │                                      │
  GPO 232/332            │  ┌──────────┐    ┌──────────────┐   │
  3-core cord            │  │  12 V DC │    │   Boost      │   │
  ┌─────────┐            │  │  Supply  │    │   Converter  │   │
  │         │  Red ──────┼──┤  + Sense ├────┤   5V → 50V   │   │
  │ Line A  │            │  │  Resistor│    │              │   │
  │         │            │  └────┬─────┘    └──────┬───────┘   │
  │         │            │       │                  │           │
  │         │  White ────┼───────┤    ┌─────────────┤           │
  │ Line B  │            │       │    │   ┌─────────┴───────┐  │
  │         │            │       │    │   │    H-Bridge     │  │
  │         │  Blue ─────┼───────┼────┼───┤    (L293D)     │  │
  │ Bell    │            │       │    │   │    25 Hz AC     │  │
  │         │            │       │    │   └─────────────────┘  │
  └─────────┘            │       │    │                        │
                         │  ┌────┴────┴──────────────────────┐ │
                         │  │  Line Interface Circuit        │ │
                         │  │  • Hook / dial pulse detect    │ │
                         │  │  • Audio coupling transformer  │ │
                         │  └────────────────────────────────┘ │
                         │                                      │
                         │  ┌──────────┐                        │
                         │  │  ESP32   │  USB / 5V power        │
                         │  │  DevKit  │◄───────────────────────┤
                         │  └──────────┘                        │
                         └──────────────────────────────────────┘
```

## Detailed Circuit

### 1. DC Line Supply & Hook / Dial Pulse Detection

```
                12V
                 │
                 ├──── 470 Ω (R1, 1W)
                 │
        Line A ──┤                      Optocoupler (U1, e.g. PC817)
       (Red)     │                     ┌─────────────────────┐
                 ├────── 220 Ω (R2) ───┤ Anode    Collector ├─── 3.3V
                 │                     │            │        │
                 │                     │    LED  Phototrans  │
                 │                     │            │        │
        Line B ──┘                     │ Cathode   Emitter  ├─── GPIO 34
       (White)   ──────────────────────┤                    │     (via 10kΩ
                                       └────────────────────┘      pull-down)
```

**How it works:**

- When **on-hook**, no current flows → optocoupler LED off → GPIO 34 reads LOW.
- When **off-hook**, ~20 mA flows through R1 and the phone circuit →
  optocoupler LED on → GPIO 34 reads HIGH (via collector pull-up).
- **Dial pulses** briefly interrupt the current, producing a train of
  LOW pulses on GPIO 34 that the `RotaryDecoder` counts.

> **Component values:** R1 sets the line current.  With a 12 V supply and
> a phone DC resistance of ~150 Ω, the off-hook current is approximately
> 12 V / (470 + 150) Ω ≈ 19 mA.  R2 limits current through the
> optocoupler LED to ~(19 mA × 220/690) ≈ 6 mA.

### 2. Ring Generator

```
                            50V DC (from boost converter)
                                    │
                     ┌──────────────┤
                     │              │
                 ┌───┴───┐     ┌───┴───┐
         GPIO 16 ┤ IN1   │     │   IN2 ├ GPIO 17
                 │       │     │       │
         GPIO 4  ┤ EN    │     │       │
                 │  L293D H-Bridge     │
                 │       │     │       │
                 │  OUT1 │     │ OUT2  │
                 └───┬───┘     └───┬───┘
                     │              │
                     ├──── Bell ────┘
                     │     Wire
                     │   (Blue)
                Line B ──────
                (White)
```

**How it works:**

- A boost converter module (e.g. XL6009 or MT3608 based) steps 5 V up to ~50 V DC.
- The L293D H-bridge alternates polarity at 25 Hz, creating square-wave AC.
- This AC is applied between the **bell wire** (blue) and **Line B** (white).
- Inside the GPO 332 the 2 µF capacitor passes AC to the bell coils.
- For the GPO 232 the signal drives an external Bellset No. 26 the same way.

> **Safety:** 50 V is within SELV (Safety Extra-Low Voltage) limits but can
> still tingle.  The boost converter should be disabled (EN pin low) whenever
> ringing is not active.  The firmware controls this via `PIN_RING_EN`.

### 3. Audio Coupling

```
                       Audio Transformer (1:1, 600 Ω)
                      ┌───────────┬───────────┐
                      │ Primary   │ Secondary │
        Line A ───────┤           │           ├─── 1 µF ─── GPIO 36 (ADC)
        (after R1)    │     ◯     │     ◯     │             (mic input)
                      │           │           │
        Line B ───────┤           │           ├─── 1 µF ─── GPIO 25 (DAC)
                      └───────────┴───────────┘             (earpiece output)
```

**How it works:**

- The transformer isolates the ESP32 from the phone line DC.
- **Receive path:** AC audio from the carbon microphone (modulating the
  line current) appears on the transformer secondary → coupling capacitor
  → ESP32 ADC (GPIO 36).
- **Transmit path:** ESP32 DAC (GPIO 25) → coupling capacitor →
  transformer secondary → primary injects AC onto the phone line →
  phone's induction coil → earpiece.
- A 600 Ω telephone-grade transformer is ideal; a small 1:1 audio
  transformer (e.g. Bourns LM-NP-1001) works well.

### 4. Power Supply

```
  USB 5V ──┬──── ESP32 DevKit (on-board 3.3V regulator)
            │
            ├──── Boost Converter Module ──── 50V DC (ring generator)
            │
            └──── 12V DC-DC Boost Module ──── Line supply
                  (or external 12V adapter)
```

- The ESP32 dev-kit is powered by USB (5 V).
- A separate 12 V source (or a boost module from 5 V) powers the phone line.
- A second boost module provides ~50 V for the ring generator.
- Both high-voltage rails should be switched off when not needed to
  conserve power and for safety.

## Full Pin Summary

| ESP32 GPIO | Function | Direction | Notes |
|------------|----------|-----------|-------|
| 34 | Line sense (hook/dial) | Input | ADC1_CH6, input-only |
| 36 | Audio in (microphone) | Input | ADC1_CH0 (VP), input-only |
| 25 | Audio out (earpiece) | Output | DAC channel 1 |
| 4 | Ring enable | Output | H-bridge EN pin |
| 16 | Ring phase A | Output | H-bridge IN1 |
| 17 | Ring phase B | Output | H-bridge IN2 |
| 2 | Status LED | Output | On-board LED |

## PCB Layout Notes

- Keep the high-voltage ring generator section away from the ESP32 and
  audio circuitry to minimise noise coupling.
- Use a ground plane and keep analogue/digital grounds joined at a single
  point near the ESP32.
- The optocoupler provides galvanic isolation between the phone line and
  the ESP32 logic — do not bridge the isolation barrier with other traces.
- Audio transformer should be as close to the phone line terminals as
  practical.
