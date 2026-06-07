# Wiring Guide

## Before You Start

1. **Identify your phone model.** The GPO 232 has no internal bell; the 332
   does.  This affects how you connect the blue (bell) wire.
2. **Check the cord wires.** If the phone has been restored, wire colours may
   differ.  Use a multimeter to identify:
   - **Line pair:** two wires that show ~150-200 Ω when the handset is lifted
     (hook switch closed).
   - **Bell wire:** the third wire, which shows high impedance to both line
     wires when on-hook (connected through a 2 µF capacitor in the 332).
3. **Do not modify the phone.**  All connections are made to the free ends of
   the 3-core cord.

## Step-by-Step Wiring

### Phone Cord → Terminal Block

Strip ~5 mm of insulation from each wire of the 3-core cord and connect to
the board's 3-way screw terminal:

```
Terminal 1  ←  Red wire   (Line A)
Terminal 2  ←  White wire (Line B)
Terminal 3  ←  Blue wire  (Bell)
```

### Terminal Block → Circuit Board

Follow the schematic in [`schematic.md`](schematic.md).  Summary:

```
Terminal 1 (Line A) ──── R1 (470 Ω) ──── +12V supply
                    └─── R2 (220 Ω) ──── Optocoupler anode
                    └─── Audio transformer primary pin 1

Terminal 2 (Line B) ──── Optocoupler cathode (via phone return)
                    └─── Audio transformer primary pin 2
                    └─── H-bridge OUT2 (for ringing)
                    └─── GND reference for line circuit

Terminal 3 (Bell)   ──── H-bridge OUT1 (for ringing)
```

### ESP32 Connections

| ESP32 Pin | Connect To |
|-----------|-----------|
| GPIO 34 | Optocoupler emitter (+ 10 kΩ pull-down to GND) |
| GPIO 36 (VP) | Audio transformer secondary pin 1 (via 1 µF cap) |
| GPIO 25 | Audio transformer secondary pin 2 (via 1 µF cap) |
| GPIO 4 | L293D Enable pin (EN1,2) |
| GPIO 16 | L293D Input 1 (IN1) |
| GPIO 17 | L293D Input 2 (IN2) |
| GND | Common ground for all modules |
| 5V / VIN | L293D VCC1 (logic supply) |

### L293D Wiring

| L293D Pin | Connection |
|-----------|-----------|
| 1 (EN1,2) | GPIO 4 |
| 2 (IN1) | GPIO 16 |
| 3 (OUT1) | Terminal 3 (Bell wire) |
| 4, 5 (GND) | GND |
| 6 (OUT2) | Terminal 2 (Line B) |
| 7 (IN2) | GPIO 17 |
| 8 (VS) | +50 V from boost converter |
| 16 (VSS) | +5 V (logic) |

### Boost Converter Modules

**50 V ring supply:**
- Input: 5 V from USB
- Output: Adjust trimpot to 50 V DC (measure with multimeter)
- Connect output to L293D pin 8 (VS)
- Connect GND to common ground

**12 V line supply:**
- Input: 5 V from USB (or use an external 12 V adapter directly)
- Output: ~12 V DC
- Connect to R1 (470 Ω) which feeds Line A

## Testing Procedure

1. **Power up without the phone connected.**  Verify 12 V and 50 V rails
   with a multimeter.  Confirm the ESP32 boots and prints to serial.

2. **Connect the phone cord.**  With the handset on the cradle (on-hook),
   verify GPIO 34 reads LOW.  Send `S` via serial to check.

3. **Lift the handset.**  GPIO 34 should go HIGH.  The serial monitor
   should print `[phone] hook: OFF_HOOK` followed by `[phone] → DIAL_TONE`.
   You should hear a dial tone in the earpiece.

4. **Dial a digit.**  Turn the rotary dial to digit 5 (for example).
   The serial monitor should print `[phone] digit: 5`.

5. **Ring the bell.**  Replace the handset (on-hook).  Send `R` via
   serial.  The bell should ring with the UK cadence pattern.  Lift the
   handset to answer and stop ringing.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| No hook detection | Measure voltage across R2 with handset lifted. Should be ~1-2 V. Check optocoupler orientation. |
| Dial pulses not counted | Ensure R1/R2 values give enough current (~15-25 mA off-hook). Adjust `LINE_THRESHOLD_ON/OFF` in `config.h`. |
| Bell doesn't ring | Verify 50 V DC on boost converter output. Check L293D wiring. GPO 232 needs an external bellset. |
| No audio in earpiece | Check transformer wiring and coupling capacitors. Verify DAC output with oscilloscope or by touching GPIO 25 with a piezo buzzer. |
| Weak microphone signal | Carbon mic may need higher bias current — reduce R1 to increase line current. |
| ESP32 resets during ringing | The boost converter may cause voltage dips. Add a larger capacitor (470 µF) on the 5 V rail and decouple the boost converter input. |
