# GPO 232 & 332 Telephone — Technical Reference

## Overview

The **GPO Telephone No. 232** (1935) and **No. 332** (1937) are classic British
Post Office Bakelite rotary-dial telephones.  Both use a 3-core cord to
connect to the line, and neither requires modification for this project.

| Feature | No. 232 | No. 332 |
|---------|---------|---------|
| Internal bell | **No** — requires external Bellset No. 26 | **Yes** — bell + 2 µF capacitor fitted |
| Induction coil | No. 20 (Mk1) / No. 27 (Mk2) | No. 27 |
| DC resistance (off-hook) | ~150-200 Ω | ~150-200 Ω |
| Handset | No. 164 (carbon mic + electromagnetic receiver) | No. 164 |
| Dial | Automatic No. 10 series | Automatic No. 10 series |
| Cord | 3-core, spade-terminal or wrapped-loop | 3-core |

## 3-Core Cord Wiring

The cord from the phone terminates in three wires.  Colours may vary on
restored phones, but the original convention is:

| Wire | Colour (original) | Function | Internal terminal |
|------|-------------------|----------|-------------------|
| Line A | Red | Line pair — through hook switch to circuit | T9 (332) |
| Line B | White | Line pair — return | T1 (332) |
| Bell | Blue | Bell / earth — via 2 µF cap to bell (332) | T11 (332) |

> **GPO 232 note:** Because the 232 has no internal bell, the blue wire
> connects to the external Bellset.  If no bellset is fitted, only the red
> and white wires are needed for basic phone operation (hook, dial, audio).

## Rotary Dial Pulse Characteristics

The GPO Dial Automatic No. 10 produces:

- **Pulse rate:** 10 impulses per second (ips), tolerance 9-11 ips
- **Break/make ratio:** approximately 2:1 (66 ms break, 33 ms make)
- **Digit encoding:** N pulses = digit N (digit 0 = 10 pulses)
- **Off-normal contacts:** short-circuit the receiver during dialling to
  suppress clicks in the earpiece

From the exchange (our ESP32) side, a dialled digit appears as a series
of brief current interruptions on the line pair.

## Hook Switch

The hook switch is a set of spring contacts actuated by the handset cradle.
Placing the handset on the cradle opens the line circuit; lifting it closes
the circuit and allows DC current to flow.

- **On-hook:** open circuit (~∞ Ω across line pair)
- **Off-hook:** ~150-200 Ω (induction coil + carbon microphone bias)

## Bell Ringing

### GPO 332 (internal bell)
The bell coils are connected between T11 (bell wire) and one side of the
line pair, in series with a 2 µF capacitor.  The capacitor blocks DC and
passes the AC ringing signal.

- **Frequency:** 25 Hz (UK standard)
- **Voltage:** 50-75 V AC peak-to-peak nominal, though many bells will
  operate from as low as 30 V AC
- **UK cadence:** 400 ms ON, 200 ms OFF, 400 ms ON, 2000 ms OFF (3 s cycle)

### GPO 232 (external bellset)
The Bellset No. 26 contains the bell and ringing capacitor.  It connects
between the line pair and earth.  The same ring signal drives it.

## Carbon Microphone

The handset (Telephone No. 164) contains a carbon-granule transmitter that
requires DC bias current to operate.  In normal exchange operation this
current (~20-30 mA) is supplied by the 48 V exchange battery through the
line.  Our interface board provides this via a 12 V supply and series
resistor.  The microphone modulates the line current, producing an audio
signal.

## Earpiece / Receiver

The electromagnetic receiver in the handset is driven by audio-frequency
AC on the line, coupled through the phone's induction coil.  Nominal
impedance is around 300 Ω.

## References

- [British Telephones — Telephone No. 232](https://www.britishtelephones.com/t232.htm)
- [British Telephones — Telephone No. 332](https://www.britishtelephones.com/t332.htm)
- Diagram N332 (circuit diagram for No. 332)
- Diagram N4300 (connecting No. 232 to Bellset No. 26)
