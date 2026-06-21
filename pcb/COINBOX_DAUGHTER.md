# A+B Coin Box Daughter Board

Universal optocoupler-isolated interface board for connecting any GPO A+B
coin collecting box mechanism to the K6 GPO Exhibit carrier board.

## Board Specs

- **Size:** 45 x 35 mm, 2-layer, all through-hole
- **Power:** 3.3V from carrier board (pull-ups) + 5V from carrier board (optocoupler LED drive)
- **Isolation:** 3x optocouplers provide galvanic isolation between A+B box contacts and ESP32

## Circuit (per channel)

```
A+B box contact             Daughter board                    Carrier board
--------------+     +--------------------------------+     +--------------
              |     |  VCC_5V --> J_in pin 1 (C+)    |     |
  Switch -----+-----+- (dry contact across J_in)     |     |
              |     |  J_in pin 2 (C-) -> R(470R)    |     |
              |     |         -> Opto LED -> GND     |     |
              |     |                                |     |
              |     |  Opto collector -- R_pull(10k)--+-->--+ VCC_3V3
              |     |  Opto collector ----------------+-->--+ GPIO (36/39/35)
              |     |  Opto emitter -----------------+-->--+ GND
              |     |                                |     |
--------------+     +--------------------------------+     +--------------
```

**Contact OPEN:**  No current flows, opto LED off -> output pulled HIGH by 10k -> ESP32 reads HIGH
**Contact CLOSED:** 5V -> R(470R) -> LED = 8mA -> opto on -> output pulled LOW -> ESP32 reads LOW

## Connector Pinouts

### J1 -- Coin Sense Input (2-pin screw terminal)

| Pin | Label | Function |
|-----|-------|----------|
| 1 | C+ | VCC_5V output (drive voltage to switch) |
| 2 | C- | Switch return -> R1 -> opto LED -> GND |

Connect the A+B box balance arm (Spring Assembly No. 2) dry contacts across pins 1-2.

### J2 -- Button A Input (2-pin screw terminal)

| Pin | Label | Function |
|-----|-------|----------|
| 1 | A+ | VCC_5V output (drive voltage to switch) |
| 2 | A- | Switch return -> R2 -> opto LED -> GND |

Connect the A+B box Button A dry contacts across pins 1-2.

### J3 -- Button B Input (2-pin screw terminal)

| Pin | Label | Function |
|-----|-------|----------|
| 1 | B+ | VCC_5V output (drive voltage to switch) |
| 2 | B- | Switch return -> R3 -> opto LED -> GND |

Connect the A+B box Button B dry contacts across pins 1-2.

### J4 -- Output Header (6-pin, to carrier board)

Pin order matches carrier board J_COIN header for straight ribbon cable connection.

| Pin | Label | Connect To (carrier board J_COIN) |
|-----|-------|-----------------------------------|
| 1 | COIN | ESP32 GPIO 36 (coin sense) |
| 2 | A | ESP32 GPIO 39 (Button A) |
| 3 | B | ESP32 GPIO 35 (Button B) |
| 4 | 3V3 | +3.3V rail (powers pull-ups R4/R5/R6) |
| 5 | GND | Ground |
| 6 | 5V | +5V rail (powers optocoupler LEDs via input side) |

## Components

| Ref | Component | Value | Purpose |
|-----|-----------|-------|---------|
| U1 | Optocoupler (DIP-4) | PC817 / EL817 | Coin sense isolation |
| U2 | Optocoupler (DIP-4) | PC817 / EL817 | Button A isolation |
| U3 | Optocoupler (DIP-4) | PC817 / EL817 | Button B isolation |
| R1 | Resistor | 470R 1/4W | Coin opto LED current limit |
| R2 | Resistor | 470R 1/4W | Button A opto LED current limit |
| R3 | Resistor | 470R 1/4W | Button B opto LED current limit |
| R4 | Resistor | 10K 1/4W | Coin output pull-up |
| R5 | Resistor | 10K 1/4W | Button A output pull-up |
| R6 | Resistor | 10K 1/4W | Button B output pull-up |
| J1 | Screw terminal | 2-pos, 5.08mm | Coin sense input |
| J2 | Screw terminal | 2-pos, 5.08mm | Button A input |
| J3 | Screw terminal | 2-pos, 5.08mm | Button B input |
| J4 | Pin header | 1x6, 2.54mm | Output to carrier board |

## Wiring Guide -- Which A+B Box Contacts to Use

The A+B mechanism uses numbered spring assemblies. The exact terminal
numbering varies by mechanism variant, but the function is the same:

### Mechanism No. 14 (most common, used in BCC No. 14/14A/14D/14DD)

| Signal | Spring Assembly | What It Does |
|--------|----------------|--------------|
| **COIN (J1)** | Spring Assembly No. 2 | Balance arm -- closes when sufficient coins are in the basket |
| **BTN A (J2)** | Spring Assembly No. 1 (A-button path) | Closes momentarily when Button A is pressed |
| **BTN B (J3)** | Spring Assembly No. 1 (B-button path) | Closes momentarily when Button B is pressed |

### Mechanism No. 13 (earlier type, BCC No. 13)

Same spring assembly functions but uses bullet-style brass connectors
instead of the U-spring plug. Identify the balance arm contact and
A/B button contacts from the mechanism wiring diagram (N.215).

### General Approach (any A+B box)

1. **Identify the coin weight/balance arm contact** -- this closes when
   coins are sitting in the mechanism basket. Connect across it to J1.
2. **Identify the Button A contact** -- closes when Button A is pressed
   to collect coins. Connect across it to J2.
3. **Identify the Button B contact** -- closes when Button B is pressed
   to refund coins. Connect across it to J3.

**No external power needed** -- VCC_5V is supplied on pin 1 of each
screw terminal from the carrier board. The A+B box contacts are
simple dry switches; when they close, they bridge pin 1 to pin 2
and 8mA flows through the optocoupler LED. No external voltage
source is required.

### Testing Without an A+B Box

You can test the daughter board with a simple jumper wire or
momentary switch across any screw terminal pair. Shorting J1 should
cause the ESP32 serial output to show coin detection; shorting J2/J3
should trigger Button A/B events.

## Auto-Detection

The firmware automatically detects whether the daughter board is
installed at boot. GPIO 36/39/35 are input-only pins with no
internal pull-up on the ESP32. When the daughter board is connected,
the 10k pull-ups hold these pins HIGH. If any pin reads LOW during
the 2-second boot window (indicating an optocoupler is active), the
coin box feature is enabled.

When no daughter board is connected, the pins float and the firmware
disables all coin logic -- the phone operates exactly as a normal
exhibit phone with no coin requirement.

## Reference

- [britishtelephones.com -- Mechanism No. 14](https://www.britishtelephones.com/mech14.htm)
- [britishtelephones.com -- How A&B Works](https://www.britishtelephones.com/howaandb.htm)
- [britishtelephones.com -- Coin Box Wiring Diagrams](https://www.britishtelephones.com/ccbwiring.htm)
