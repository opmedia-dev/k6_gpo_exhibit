#!/usr/bin/env python3
"""Generate KiCad 6 PCB file for K6 GPO Exhibit carrier board (Rev 2).

Uses the pcbnew Python API to create a valid PCB with proper footprints.
Board: 100mm x 80mm, 2-layer
ESP32 DevKit V1 (30-pin, 2x15) with USB facing bottom edge.

Run with: /usr/bin/python3 generate_pcb.py
(Must use system Python for pcbnew module access)

Layout (top to bottom):
  Zone A (y 0-15mm):  Power — barrel jacks + LM2596 module
  Zone B (y 15-55mm): ESP32 centre, peripherals on left/right sides
  Zone E (y 55-80mm): Connectors — terminal, buttons, lamp, daughter board, pull-ups
"""

import pcbnew
import os

def mm(val):
    return int(val * 1e6)

BOARD_W = 100.0
BOARD_H = 80.0
OX = 100.0  # Board origin X in KiCad coords
OY = 50.0   # Board origin Y in KiCad coords

FP_LIB = "/usr/share/kicad/footprints/"


def load_fp(lib_name, fp_name):
    lib_path = os.path.join(FP_LIB, lib_name + ".pretty")
    fp = pcbnew.FootprintLoad(lib_path, fp_name)
    if fp is None:
        raise RuntimeError(f"Could not load {lib_name}:{fp_name}")
    return fp


def place(board, lib_name, fp_name, ref, value, x_mm, y_mm, angle_deg=0):
    fp = load_fp(lib_name, fp_name)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(pcbnew.wxPointMM(x_mm, y_mm))
    if angle_deg != 0:
        fp.SetOrientationDegrees(angle_deg)
    board.Add(fp)
    return fp


def text(board, txt, x, y, layer, sz=1.0, thk=0.15):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(txt)
    t.SetPosition(pcbnew.wxPointMM(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.wxSizeMM(sz, sz))
    t.SetTextThickness(mm(thk))
    board.Add(t)


def line(board, x1, y1, x2, y2, layer, w=0.15):
    seg = pcbnew.PCB_SHAPE(board)
    seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
    seg.SetStart(pcbnew.wxPointMM(x1, y1))
    seg.SetEnd(pcbnew.wxPointMM(x2, y2))
    seg.SetLayer(layer)
    seg.SetWidth(mm(w))
    board.Add(seg)


def main():
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)

    # ── Board outline ──
    cx = [(OX, OY), (OX+BOARD_W, OY), (OX+BOARD_W, OY+BOARD_H), (OX, OY+BOARD_H)]
    for i in range(4):
        x1, y1 = cx[i]; x2, y2 = cx[(i+1)%4]
        line(board, x1, y1, x2, y2, pcbnew.Edge_Cuts, 0.1)

    # ── Nets ──
    for i, n in enumerate(["", "GND", "+5V", "+3V3", "+12V", "+48V",
        "GPIO4", "GPIO16", "GPIO17", "GPIO26", "GPIO25", "GPIO22",
        "GPIO5", "GPIO23", "GPIO19", "GPIO18",
        "GPIO32", "GPIO33", "GPIO27", "GPIO14",
        "GPIO36", "GPIO39", "GPIO35", "GPIO13", "GPIO34", "GPIO2",
        "LINE_A", "LINE_B", "BELL", "OPTO_E"]):
        board.Add(pcbnew.NETINFO_ITEM(board, n, i))

    S = pcbnew.F_SilkS

    # ═══════════════════════════════════════════════════════════
    # Coordinate system: all relative to board top-left (OX, OY)
    # x grows right, y grows down
    # ═══════════════════════════════════════════════════════════

    # ── ZONE A: POWER (y = 0..15mm) ──

    # J1: 12V barrel jack — left side
    place(board, "Connector_BarrelJack", "BarrelJack_Horizontal",
          "J1", "12V_DC", OX+14, OY+10)
    text(board, "12V DC IN", OX+14, OY+3, S, 0.9, 0.12)

    # LM2596 buck module header (4-pin vertical: IN+ IN- OUT+ OUT-)
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x04_P2.54mm_Vertical",
          "U_BUCK", "LM2596", OX+42, OY+8)
    text(board, "LM2596 Buck", OX+42, OY+3, S, 0.8, 0.12)

    # J2: 48V barrel jack — right side, rotated 180
    place(board, "Connector_BarrelJack", "BarrelJack_Horizontal",
          "J2", "48V_DC", OX+86, OY+10, 180)
    text(board, "48V DC IN", OX+86, OY+3, S, 0.9, 0.12)

    # ── ZONE B: ESP32 + PERIPHERALS (y = 17..55mm) ──
    # ESP32 centered horizontally, pin 1 (USB end) at bottom

    esp_cx = OX + 50                # ESP32 centre X
    esp_row = 22.86                 # row-to-row distance (0.9")
    esp_lx = esp_cx - esp_row/2     # left socket X = 138.57
    esp_rx = esp_cx + esp_row/2     # right socket X = 161.43
    dev_off = 5.08                  # dev hole offset from socket
    dev_lx = esp_lx - dev_off       # dev left X = 133.49
    dev_rx = esp_rx + dev_off       # dev right X = 166.51

    # ESP32 pin sockets centred at y = OY+36 (gives pin1 at y+53.8, pin15 at y+18.2)
    esp_cy = OY + 36
    pin1_y = esp_cy + 7*2.54        # bottom pin (USB end) = OY + 53.78
    pin15_y = esp_cy - 7*2.54       # top pin (antenna end) = OY + 18.22

    # Left and right ESP32 sockets
    place(board, "Connector_PinSocket_2.54mm", "PinSocket_1x15_P2.54mm_Vertical",
          "J_ESP_L", "ESP32_Left", esp_lx, esp_cy)
    place(board, "Connector_PinSocket_2.54mm", "PinSocket_1x15_P2.54mm_Vertical",
          "J_ESP_R", "ESP32_Right", esp_rx, esp_cy)

    # Dev access holes (pin headers parallel to ESP32 sockets)
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x15_P2.54mm_Vertical",
          "J_DEV_L", "DEV_Left", dev_lx, esp_cy)
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x15_P2.54mm_Vertical",
          "J_DEV_R", "DEV_Right", dev_rx, esp_cy)

    # C2: decoupling cap near ESP32 VIN (bottom left of ESP32)
    place(board, "Capacitor_THT", "C_Disc_D3.0mm_W1.6mm_P2.50mm",
          "C2", "100nF", esp_lx - 4, pin1_y)

    # USB direction label
    text(board, "USB -->", esp_cx, pin1_y + 3, S, 0.8, 0.12)

    # ESP32 left pin labels (bottom=pin1=VIN to top=pin15=EN)
    left_labels = ["VIN","GND","D13","D12","D14","D27","D26","D25",
                   "D33","D32","D35","D34","VN","VP","EN"]
    for i, lbl in enumerate(left_labels):
        text(board, lbl, dev_lx - 4, pin1_y - i*2.54, S, 0.55, 0.08)

    # ESP32 right pin labels
    right_labels = ["3V3","GND","D15","D2","D4","RX2","TX2","D5",
                    "D18","D19","D21","RX0","TX0","D22","D23"]
    for i, lbl in enumerate(right_labels):
        text(board, lbl, dev_rx + 4, pin1_y - i*2.54, S, 0.55, 0.08)

    # ── LEFT PERIPHERALS (x = 0..28mm) ──

    # PC817 optocoupler (DIP-4, 12.7 x 9mm)
    place(board, "Package_DIP", "DIP-4_W7.62mm",
          "U1", "PC817", OX+10, OY+22)
    text(board, "PC817", OX+10, OY+18, S, 0.7, 0.1)
    text(board, "pin1 dot", OX+5, OY+20, S, 0.5, 0.08)

    # R1: 470Ω 1W — same pad size as R2/R3 for consistency
    place(board, "Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
          "R1", "470R_1W", OX+8, OY+30)

    # R2: 220Ω — horizontal
    place(board, "Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
          "R2", "220R", OX+8, OY+34)

    # R3: 10kΩ — horizontal
    place(board, "Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
          "R3", "10K", OX+8, OY+38)

    # MAX98357A DAC module: 7-pin header (bottom) + 2-pin screw terminal (speaker)
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x07_P2.54mm_Vertical",
          "U_DAC", "MAX98357A", OX+10, OY+46)
    text(board, "DAC", OX+6, OY+46, S, 0.6, 0.1)
    # DAC speaker screw terminal (2-pin, above the header)
    place(board, "TerminalBlock", "TerminalBlock_bornier-2_P5.08mm",
          "J_SPK", "Speaker", OX+10, OY+40)
    text(board, "SPK+/-", OX+6, OY+39, S, 0.5, 0.08)

    # Audio transformer EI-14 (2x2 pins: primary left, secondary right)
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_2x02_P2.54mm_Vertical",
          "T1", "600R_XFMR", OX+20, OY+46)
    text(board, "XFMR", OX+20, OY+42, S, 0.6, 0.1)
    text(board, "Pri  Sec", OX+20, OY+44, S, 0.4, 0.06)

    # SD card module (6-pin header: CS SCK MOSI MISO VCC GND)
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x06_P2.54mm_Vertical",
          "U_SD", "SD_Card", OX+24, OY+38)
    text(board, "SD Card", OX+24, OY+32, S, 0.6, 0.1)
    text(board, "CS SCK MOSI", OX+24, OY+34, S, 0.4, 0.06)
    text(board, "MISO VCC GND", OX+24, OY+36, S, 0.4, 0.06)

    # ── RIGHT PERIPHERALS (x = 72..100mm) ──

    # L293D H-bridge (DIP-16, 13.7 x 24.3mm)
    place(board, "Package_DIP", "DIP-16_W7.62mm",
          "U2", "L293D", OX+85, OY+30)
    text(board, "L293D", OX+85, OY+20, S, 0.8, 0.12)
    text(board, "pin1 notch", OX+79, OY+22, S, 0.5, 0.08)
    text(board, "48V on pin 8", OX+85, OY+45, S, 0.5, 0.08)

    # C1: decoupling cap near L293D VSS (pin 16)
    place(board, "Capacitor_THT", "C_Disc_D3.0mm_W1.6mm_P2.50mm",
          "C1", "100nF", OX+92, OY+22)

    # ── ZONE E: CONNECTORS (y = 57..80mm) ──

    # 3-way screw terminal for phone cord
    place(board, "TerminalBlock", "TerminalBlock_bornier-3_P5.08mm",
          "J_PHONE", "Phone", OX+12, OY+62)
    text(board, "PHONE", OX+12, OY+58, S, 0.7, 0.1)
    text(board, "A  B  Bell", OX+12, OY+68, S, 0.5, 0.08)

    # 4x buttons as 2-pin headers (GPIO + GND)
    btn_y = OY + 62
    btn_labels = [("SW1","RING",OX+28), ("SW2","CANCEL",OX+36),
                  ("SW3","RESET",OX+44), ("SW4","MODE",OX+52)]
    for ref, label, bx in btn_labels:
        place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical",
              ref, label, bx, btn_y)
        text(board, label, bx, btn_y + 4, S, 0.6, 0.1)

    # Panel lamp connector (2-pin)
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical",
          "J_LAMP", "Lamp", OX+62, btn_y)
    text(board, "LAMP", OX+62, btn_y + 4, S, 0.6, 0.1)

    # Pull-up resistors R4/R5/R6 — vertical orientation near daughter header
    pu_y = OY + 62
    place(board, "Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
          "R4", "10K", OX+72, pu_y, 90)
    place(board, "Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
          "R5", "10K", OX+76, pu_y, 90)
    place(board, "Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
          "R6", "10K", OX+80, pu_y, 90)
    text(board, "R4 R5 R6", OX+76, OY+57, S, 0.5, 0.08)
    text(board, "10K pull-ups", OX+76, OY+59, S, 0.5, 0.08)

    # 6-pin daughter board header
    place(board, "Connector_PinHeader_2.54mm", "PinHeader_1x06_P2.54mm_Vertical",
          "J_COIN", "A+B", OX+90, OY+62)
    text(board, "DAUGHTER", OX+90, OY+57, S, 0.6, 0.1)
    text(board, "A+B COIN", OX+90, OY+59, S, 0.6, 0.1)

    # ── MOUNTING HOLES (4 corners, 4mm inset) ──
    mi = 4.0
    for i, (mx, my) in enumerate([
        (OX+mi, OY+mi), (OX+BOARD_W-mi, OY+mi),
        (OX+mi, OY+BOARD_H-mi), (OX+BOARD_W-mi, OY+BOARD_H-mi)]):
        place(board, "MountingHole", "MountingHole_3.2mm_M3",
              f"H{i+1}", "M3", mx, my)

    # ═══════════════════════════════════════════════════════════
    # SILKSCREEN LABELS
    # ═══════════════════════════════════════════════════════════

    text(board, "ZONE A: POWER", OX+50, OY+1.5, S, 1.2, 0.18)
    text(board, "ZONE B: ESP32 + PERIPHERALS", OX+50, OY+16, S, 1.0, 0.15)
    text(board, "ZONE E: CONNECTORS", OX+50, OY+55, S, 1.0, 0.15)

    # Zone separator lines
    line(board, OX, OY+15, OX+BOARD_W, OY+15, S, 0.15)
    line(board, OX, OY+55, OX+BOARD_W, OY+55, S, 0.15)

    # Board title (bottom edge)
    text(board, "K6 GPO Exhibit", OX+50, OY+BOARD_H-4, S, 1.5, 0.25)
    text(board, "Carrier Board Rev 2", OX+50, OY+BOARD_H-1.5, S, 0.9, 0.12)

    # ═══════════════════════════════════════════════════════════
    # GROUND ZONE (back copper pour)
    # ═══════════════════════════════════════════════════════════
    gnd = board.FindNet("GND")
    if gnd:
        zone = pcbnew.ZONE(board)
        zone.SetNet(gnd)
        zone.SetLayer(pcbnew.B_Cu)
        zone.SetIsRuleArea(False)
        zone.SetDoNotAllowCopperPour(False)
        zone.SetMinThickness(mm(0.25))
        zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        zone.SetThermalReliefGap(mm(0.508))
        zone.SetThermalReliefSpokeWidth(mm(0.508))
        o = zone.Outline()
        o.NewOutline()
        o.Append(mm(OX+1), mm(OY+1))
        o.Append(mm(OX+BOARD_W-1), mm(OY+1))
        o.Append(mm(OX+BOARD_W-1), mm(OY+BOARD_H-1))
        o.Append(mm(OX+1), mm(OY+BOARD_H-1))
        board.Add(zone)

    # ═══════════════════════════════════════════════════════════
    # SAVE
    # ═══════════════════════════════════════════════════════════
    out = "/home/ubuntu/repos/k6_gpo_exhibit/pcb/k6_carrier_rev2.kicad_pcb"
    board.Save(out)
    print(f"PCB saved to: {out}")
    print(f"Board: {BOARD_W} x {BOARD_H} mm, 2-layer")
    print("ESP32 DevKit V1 (30-pin) with USB toward bottom edge")
    print()
    print("Layout:")
    print("  Zone A (top):    J1 12V, U_BUCK LM2596, J2 48V")
    print("  Zone B (centre): ESP32 sockets + dev holes (centre)")
    print("     Left side:    U1 PC817, R1-R3, U_DAC, T1, U_SD")
    print("     Right side:   U2 L293D, C1")
    print("  Zone E (bottom): J_PHONE, SW1-4 buttons (2-pin), J_LAMP, R4-R6, J_COIN")
    print("  Corners:         H1-H4 mounting holes")


if __name__ == "__main__":
    main()
