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
4. **The ESP32 board lives in a separate box**, connected to the phone via an
   extended multi-core cable.

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
                    └─── Audio transformer secondary pin 1

Terminal 2 (Line B) ──── Optocoupler cathode (via phone return)
                    └─── Audio transformer secondary pin 2
                    └─── H-bridge OUT2 (for ringing)
                    └─── GND reference for line circuit

Terminal 3 (Bell)   ──── H-bridge OUT1 (for ringing)
```

### ESP32 Connections

| ESP32 Pin | Connect To |
|-----------|-----------|
| GPIO 34 | Optocoupler emitter (+ 10 kΩ pull-down to GND) |
| GPIO 26 | MAX98357A BCLK |
| GPIO 25 | MAX98357A LRC |
| GPIO 22 | MAX98357A DIN |
| GPIO 5 | SD card module CS |
| GPIO 23 | SD card module MOSI |
| GPIO 19 | SD card module MISO |
| GPIO 18 | SD card module SCK |
| GPIO 4 | L293D Enable pin (EN1,2) |
| GPIO 16 | L293D Input 1 (IN1) |
| GPIO 17 | L293D Input 2 (IN2) |
| GPIO 32 | RING button (other terminal to GND) |
| GPIO 33 | CANCEL button (other terminal to GND) |
| GPIO 27 | RESET button (other terminal to GND) |
| GND | Common ground for all modules |
| 5V / VIN | L293D VCC1 + MAX98357A VIN |

### MAX98357A I2S DAC

| MAX98357A Pin | Connection |
|--------------|-----------|
| VIN | 5 V |
| GND | GND |
| BCLK | GPIO 26 |
| LRC | GPIO 25 |
| DIN | GPIO 22 |
| L+ | Audio transformer primary pin 1 |
| L- | Audio transformer primary pin 2 |

### SD Card Module

| SD Module Pin | Connection |
|--------------|-----------|
| VCC | 3.3 V |
| GND | GND |
| CS | GPIO 5 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCK | GPIO 18 |

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

### Control Panel Buttons

Wire each button between the ESP32 GPIO pin and GND.  No external resistors
are needed — the firmware enables internal pull-ups.

```
GPIO 32 ──── [RING button]   ──── GND
GPIO 33 ──── [CANCEL button] ──── GND
GPIO 27 ──── [RESET button]  ──── GND
```

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

## SD Card Preparation

1. Format a micro-SD card as **FAT32**
2. Create the directory structure:
   ```
   /system/dialtone.mp3
   /system/busy.mp3
   /system/not_recognised.mp3
   /history/001.mp3
   /history/002.mp3
   /numbers/999.mp3
   /numbers/100.mp3
   ```
3. Insert the card into the SD module before powering on

## Testing Procedure

1. **Power up without the phone connected.**  Verify 12 V and 50 V rails
   with a multimeter.  Confirm the ESP32 boots and prints to serial.
   Check `[audio] SD card ready` appears in the serial output.

2. **Connect the phone cord.**  With the handset on the cradle (on-hook),
   verify GPIO 34 reads LOW.  Send `S` via serial to check status.

3. **Lift the handset.**  The serial monitor should print
   `[phone] → DIAL_TONE`.  You should hear the dial tone MP3 playing
   in the earpiece.

4. **Dial a digit.**  Turn the rotary dial to digit 5 (for example).
   The serial monitor should print `[phone] digit: 5`.  After the
   inter-digit timeout, the matching MP3 plays (or "not recognised").

5. **Ring the bell.**  Replace the handset.  Send `R` via serial or
   press the RING button.  The bell should ring.  Lift the handset —
   a random history track should play.

6. **Test buttons.**
   - Press CANCEL during ringing → ringing stops
   - Press RESET → ESP32 reboots

7. **Auto-ring.**  Wait for the timer (default 5-30 min, adjustable in
   `config.h`) or send `A` via serial to toggle.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| No hook detection | Measure voltage across R2 with handset lifted. Should be ~1-2 V. Check optocoupler orientation. |
| Dial pulses not counted | Ensure R1/R2 values give enough current (~15-25 mA off-hook). Adjust `LINE_THRESHOLD_ON/OFF` in `config.h`. |
| Bell doesn't ring | Verify 50 V DC on boost converter output. Check L293D wiring. GPO 232 needs an external bellset. |
| No audio in earpiece | Check MAX98357A wiring. Verify I2S pins. Check transformer connections. Try serial command `V9` for max volume. |
| SD card not detected | Check SPI wiring. Ensure card is FAT32. Try a different card. Check `[audio] SD card init failed` in serial output. |
| No MP3 playback | Verify file paths on SD card match expected layout (`/system/`, `/history/`, `/numbers/`). Check file is valid MP3. |
| Buttons don't respond | Check wiring to GND. Verify GPIO pin numbers match `config.h`. |
| ESP32 resets during ringing | The boost converter may cause voltage dips. Add a larger capacitor (470 µF) on the 5 V rail. |
