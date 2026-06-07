# Hardware Schematic — ESP32 GPO Phone Interface

## Block Diagram

```
                         ┌──────────────────────────────────────────┐
                         │      ESP32 Interface Board               │
                         │      (in separate enclosure)             │
  GPO 232/332            │                                          │
  3-core cord            │  ┌──────────┐    ┌──────────────┐       │
  (extended)             │  │  12 V DC │    │   Boost      │       │
  ┌─────────┐            │  │  Supply  │    │   Converter  │       │
  │         │  Red ──────┼──┤  + Sense ├────┤   5V → 50V   │       │
  │ Line A  │            │  │  Resistor│    │              │       │
  │         │            │  └────┬─────┘    └──────┬───────┘       │
  │         │            │       │                  │               │
  │         │  White ────┼───────┤    ┌─────────────┤               │
  │ Line B  │            │       │    │   ┌─────────┴───────┐      │
  │         │            │       │    │   │    H-Bridge     │      │
  │         │  Blue ─────┼───────┼────┼───┤    (L293D)     │      │
  │ Bell    │            │       │    │   │    25 Hz AC     │      │
  │         │            │       │    │   └─────────────────┘      │
  └─────────┘            │       │    │                            │
                         │  ┌────┴────┴──────────────────────┐     │
                         │  │  Line Interface Circuit        │     │
                         │  │  • Hook / dial pulse detect    │     │
                         │  │  • Audio coupling transformer  │     │
                         │  │  • MAX98357A I2S DAC output    │     │
                         │  └────────────────────────────────┘     │
                         │                                          │
                         │  ┌──────────┐  ┌─────────┐  ┌────────┐ │
                         │  │  ESP32   │  │ SD Card │  │ Control│ │
                         │  │  DevKit  │  │ Module  │  │ Buttons│ │
                         │  └──────────┘  └─────────┘  └────────┘ │
                         │       │                                  │
                         │  USB power + serial                     │
                         └──────────────────────────────────────────┘
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

### 3. Audio Output (I2S DAC → Phone Line)

```
        ESP32                    MAX98357A            Audio Transformer
    ┌───────────┐             ┌─────────────┐      ┌───────────┬───────────┐
    │ GPIO 26 ──┼── BCLK ──►│ BCLK        │      │ Primary   │ Secondary │
    │ GPIO 25 ──┼── LRCLK ─►│ LRC     L+ ├─────►┤           │           ├─── Line A
    │ GPIO 22 ──┼── DIN ───►│ DIN     L- ├─────►┤     ◯     │     ◯     │
    │ 5V ───────┼── VIN ───►│ VIN        │      │           │           │
    │ GND ──────┼── GND ───►│ GND        │      │           │           ├─── Line B
    └───────────┘             └─────────────┘      └───────────┴───────────┘
```

**How it works:**

- The ESP32 sends I2S audio data to the MAX98357A DAC module.
- The DAC output (L+/L-) feeds the primary of the audio transformer.
- The transformer secondary is connected across the phone line pair.
- Audio couples through the phone's induction coil to the earpiece.
- A 10 Ω resistor in series with the transformer primary may be needed
  to limit current and match impedance.

### 4. SD Card Module (SPI)

```
        ESP32               SD Card Module
    ┌───────────┐          ┌─────────────┐
    │ GPIO 5  ──┼── CS ──►│ CS          │
    │ GPIO 23 ──┼── MOSI ►│ MOSI        │
    │ GPIO 19 ──┼── MISO ◄│ MISO        │
    │ GPIO 18 ──┼── SCK ─►│ SCK         │
    │ 3.3V ─────┼── VCC ─►│ VCC         │
    │ GND ──────┼── GND ─►│ GND         │
    └───────────┘          └─────────────┘
```

### 5. Control Panel Buttons

```
    GPIO 32 ──── [BTN RING] ──── GND
    GPIO 33 ──── [BTN CANCEL] ── GND
    GPIO 27 ──── [BTN RESET] ─── GND

    (ESP32 internal pull-ups enabled; buttons connect pin to GND)
```

The buttons are mounted on the operator's control box, connected to the
ESP32 board via the multi-core cable alongside the phone cord.

### 6. Power Supply

```
  USB 5V ──┬──── ESP32 DevKit (on-board 3.3V regulator)
            │
            ├──── MAX98357A VIN
            │
            ├──── Boost Converter Module ──── 50V DC (ring generator)
            │
            └──── 12V DC-DC Boost Module ──── Line supply
                  (or external 12V adapter)
```

## Full Pin Summary

| ESP32 GPIO | Function | Direction | Notes |
|------------|----------|-----------|-------|
| 34 | Line sense (hook/dial) | Input | ADC1_CH6, input-only |
| 26 | I2S BCLK | Output | To MAX98357A |
| 25 | I2S LRCLK | Output | To MAX98357A |
| 22 | I2S DOUT | Output | To MAX98357A |
| 5 | SD card CS | Output | SPI chip select |
| 23 | SD card MOSI | Output | SPI data out |
| 19 | SD card MISO | Input | SPI data in |
| 18 | SD card SCK | Output | SPI clock |
| 4 | Ring enable | Output | H-bridge EN pin |
| 16 | Ring phase A | Output | H-bridge IN1 |
| 17 | Ring phase B | Output | H-bridge IN2 |
| 32 | Button: RING | Input | Active-low, internal pull-up |
| 33 | Button: CANCEL | Input | Active-low, internal pull-up |
| 27 | Button: RESET | Input | Active-low, internal pull-up |
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
- Run the extended phone cord and button wiring through shielded or
  twisted-pair cable to reduce interference pickup.
