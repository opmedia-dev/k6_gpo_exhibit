#!/usr/bin/env python3
"""Generate KiCad 10 project files for the A+B Coin Box daughter board.

Creates:
  k6_coinbox_daughter.kicad_pro   – project file
  k6_coinbox_daughter.kicad_sch   – schematic
  k6_coinbox_daughter.kicad_pcb   – PCB layout (40 × 30 mm, 2-layer)

Universal daughter board for any GPO A+B coin collecting box mechanism.
3 optocoupler-isolated inputs (coin sense, Button A, Button B) connect
to the carrier board ESP32 GPIOs 36/39/35 via a 6-pin header.

Circuit per channel:
  A+B contact ──► [screw terminal] ──► R_in (470Ω) ──► Opto LED ──► GND_in
  ESP32 side:  VCC ──► R_pull (10kΩ) ──► Opto collector ──► output pin
                                         Opto emitter ──► GND

When A+B contact is OPEN:  opto LED off, output pulled HIGH by R_pull
When A+B contact is CLOSED: opto LED on, output pulled LOW by opto transistor

The ESP32 input-only pins (36/39/35) have no internal pull-up, so the
10kΩ pull-up on this board is essential.
"""
import uuid, json, textwrap

def uid():
    return str(uuid.uuid4())

# ─── Board dimensions ────────────────────────────────────────────────
BOARD_W = 45.0   # mm
BOARD_H = 35.0
ORIGIN_X = 130.0
ORIGIN_Y = 90.0
CORNER_R = 1.5
MOUNT_HOLE_D = 2.5
MOUNT_INSET = 3.0

# ─── Net list ─────────────────────────────────────────────────────────
NETS = {
    0: "",
    1: "VCC_3V3",       # 3.3V from carrier board (pull-ups)
    2: "GND",
    3: "COIN_SENSE",    # output to ESP32 GPIO 36
    4: "BTN_A",         # output to ESP32 GPIO 39
    5: "BTN_B",         # output to ESP32 GPIO 35
    6: "COIN_IN_A",     # screw terminal input from A+B box
    7: "COIN_IN_B",     # screw terminal return
    8: "BTNA_IN_A",     # screw terminal input
    9: "BTNA_IN_B",     # screw terminal return
    10: "BTNB_IN_A",    # screw terminal input
    11: "BTNB_IN_B",    # screw terminal return
    12: "OPTO1_CA",     # opto 1 cathode / R junction
    13: "OPTO2_CA",     # opto 2 cathode / R junction
    14: "OPTO3_CA",     # opto 3 cathode / R junction
    15: "VCC_5V",       # 5V from carrier board (optocoupler LED drive)
}

def net_defs():
    lines = []
    for nid, name in sorted(NETS.items()):
        lines.append(f'  (net {nid} "{name}")')
    return "\n".join(lines)

# ─── Helpers ─────────────────────────────────────────────────────────
def fp_pad_th(number, x, y, drill=1.0, size=1.7, shape="circle", net_id=0, net_name=""):
    s = "oval" if shape == "oval" else "circle"
    return (f'  (pad "{number}" thru_hole {s} (at {x:.3f} {y:.3f}) '
            f'(size {size} {size}) (drill {drill}) '
            f'(layers "*.Cu" "*.Mask") '
            f'(net {net_id} "{net_name}") (tstamp {uid()}))')

def fp_pad_np(x, y, drill=2.5):
    return (f'  (pad "" np_thru_hole circle (at {x:.3f} {y:.3f}) '
            f'(size {drill} {drill}) (drill {drill}) '
            f'(layers "*.Cu" "*.Mask") (tstamp {uid()}))')

def silk_text(text, x, y, layer="F.SilkS", size=0.8, thickness=0.12):
    return (f'  (gr_text "{text}" (at {x:.3f} {y:.3f}) (layer "{layer}")\n'
            f'    (effects (font (size {size} {size}) (thickness {thickness})))\n'
            f'    (tstamp {uid()}))')

def ref_text(ref, x, y, layer="F.SilkS", size=0.8, thickness=0.12):
    return (f'  (fp_text reference "{ref}" (at {x:.3f} {y:.3f}) (layer "{layer}")\n'
            f'    (effects (font (size {size} {size}) (thickness {thickness})))\n'
            f'    (tstamp {uid()}))')

def val_text(val, x, y, layer="F.Fab", size=0.8, thickness=0.12):
    return (f'  (fp_text value "{val}" (at {x:.3f} {y:.3f}) (layer "{layer}")\n'
            f'    (effects (font (size {size} {size}) (thickness {thickness})))\n'
            f'    (tstamp {uid()}))')

def make_footprint(ref, val, x, y, angle, pads_text, extra_drawing=""):
    return (
        f'  (footprint "K6_COINBOX:{ref}" (layer "F.Cu")\n'
        f'    (tstamp {uid()})\n'
        f'    (at {x:.3f} {y:.3f} {angle})\n'
        f'    {ref_text(ref, 0, -2)}\n'
        f'    {val_text(val, 0, 2)}\n'
        f'{extra_drawing}'
        f'{pads_text}\n'
        f'  )'
    )

def resistor_th(ref, val, x, y, angle, net_1, net_2, pitch=7.62):
    pads = []
    n1 = NETS.get(net_1, "")
    n2 = NETS.get(net_2, "")
    pads.append(fp_pad_th(1, -pitch/2, 0, net_id=net_1, net_name=n1))
    pads.append(fp_pad_th(2, pitch/2, 0, net_id=net_2, net_name=n2))
    body = (f'    (fp_line (start {-pitch/2+1:.3f} -1.0) (end {pitch/2-1:.3f} -1.0) '
            f'(stroke (width 0.12) (type default)) (layer "F.SilkS") (tstamp {uid()}))\n'
            f'    (fp_line (start {-pitch/2+1:.3f} 1.0) (end {pitch/2-1:.3f} 1.0) '
            f'(stroke (width 0.12) (type default)) (layer "F.SilkS") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), body)

def dip4_opto(ref, val, x, y, angle, net_map, row_spacing=7.62, pitch=2.54):
    """4-pin DIP optocoupler (PC817/EL817): pin 1=Anode, 2=Cathode, 3=Emitter, 4=Collector."""
    pads = []
    # Left side: pins 1 (top), 2 (bottom)
    for i, pin in enumerate([1, 2]):
        py = (i - 0.5) * pitch
        nid = net_map.get(pin, 0)
        pads.append(fp_pad_th(pin, -row_spacing/2, py, drill=0.8, size=1.6,
                              net_id=nid, net_name=NETS.get(nid, "")))
    # Right side: pins 4 (top), 3 (bottom) — DIP numbering
    for i, pin in enumerate([4, 3]):
        py = (i - 0.5) * pitch
        nid = net_map.get(pin, 0)
        pads.append(fp_pad_th(pin, row_spacing/2, py, drill=0.8, size=1.6,
                              net_id=nid, net_name=NETS.get(nid, "")))
    crt_h = 2 * pitch + 1.0
    crt = (f'    (fp_rect (start {-(row_spacing/2+1.5):.3f} {-(crt_h/2):.3f}) '
           f'(end {(row_spacing/2+1.5):.3f} {crt_h/2:.3f}) (stroke (width 0.05) (type default)) '
           f'(fill none) (layer "F.CrtYd") (tstamp {uid()}))\n')
    # Pin 1 dot
    dot = (f'    (fp_circle (center {-(row_spacing/2+0.5):.3f} {-pitch/2:.3f}) (end {-(row_spacing/2+0.2):.3f} {-pitch/2:.3f}) '
           f'(stroke (width 0.12) (type default)) (fill solid) (layer "F.SilkS") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), crt + dot)

def screw_terminal_2(ref, val, x, y, angle, net_1, net_2, pitch=5.08):
    pads = []
    n1 = NETS.get(net_1, "")
    n2 = NETS.get(net_2, "")
    pads.append(fp_pad_th(1, -pitch/2, 0, drill=1.3, size=2.5, net_id=net_1, net_name=n1))
    pads.append(fp_pad_th(2, pitch/2, 0, drill=1.3, size=2.5, net_id=net_2, net_name=n2))
    return make_footprint(ref, val, x, y, angle, "\n".join(pads))

def pin_header_1xN(n, ref, val, x, y, angle, net_map, pitch=2.54):
    pads = []
    for i in range(n):
        py = (i - (n-1)/2.0) * pitch
        nid = net_map.get(i+1, 0)
        pads.append(fp_pad_th(i+1, 0, py, net_id=nid, net_name=NETS.get(nid, "")))
    crt_h = n * pitch + 1.0
    crt = (f'    (fp_rect (start -1.5 {-(crt_h/2):.3f}) '
           f'(end 1.5 {crt_h/2:.3f}) (stroke (width 0.05) (type default)) '
           f'(fill none) (layer "F.CrtYd") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), crt)

def mounting_hole(ref, x, y):
    pad = fp_pad_np(0, 0, drill=MOUNT_HOLE_D)
    return make_footprint(ref, "MH", x, y, 0, pad)

# ═════════════════════════════════════════════════════════════════════
#  BUILD PCB
# ═════════════════════════════════════════════════════════════════════
OX = ORIGIN_X
OY = ORIGIN_Y

def build_pcb():
    footprints = []
    labels = []

    # ── Screw terminals (left side) — A+B box contact inputs ──
    # J1: Coin sense contacts
    footprints.append(screw_terminal_2("J1", "COIN", OX + 6, OY + 7, 0, 6, 7))
    labels.append(silk_text("COIN", OX + 6, OY + 3.5))
    labels.append(silk_text("C+", OX + 3.5, OY + 7))
    labels.append(silk_text("C-", OX + 8.5, OY + 7))

    # J2: Button A contacts
    footprints.append(screw_terminal_2("J2", "BTN_A", OX + 6, OY + 15, 0, 8, 9))
    labels.append(silk_text("BTN A", OX + 6, OY + 11.5))
    labels.append(silk_text("A+", OX + 3.5, OY + 15))
    labels.append(silk_text("A-", OX + 8.5, OY + 15))

    # J3: Button B contacts
    footprints.append(screw_terminal_2("J3", "BTN_B", OX + 6, OY + 23, 0, 10, 11))
    labels.append(silk_text("BTN B", OX + 6, OY + 19.5))
    labels.append(silk_text("B+", OX + 3.5, OY + 23))
    labels.append(silk_text("B-", OX + 8.5, OY + 23))

    # ── Input current-limiting resistors (470Ω each) ──
    # R1: Coin sense LED current limiter
    footprints.append(resistor_th("R1", "470R", OX + 16, OY + 7, 0, 6, 12))

    # R2: Button A LED current limiter
    footprints.append(resistor_th("R2", "470R", OX + 16, OY + 15, 0, 8, 13))

    # R3: Button B LED current limiter
    footprints.append(resistor_th("R3", "470R", OX + 16, OY + 23, 0, 10, 14))

    # ── Optocouplers (centre) ──
    # U1: Coin sense opto (PC817/EL817)
    # Pin 1=Anode (from R1), 2=Cathode (to J1 return), 3=Emitter (GND), 4=Collector (COIN_SENSE)
    footprints.append(dip4_opto("U1", "PC817", OX + 26, OY + 7, 0,
                                {1: 12, 2: 7, 3: 2, 4: 3}))

    # U2: Button A opto
    footprints.append(dip4_opto("U2", "PC817", OX + 26, OY + 15, 0,
                                {1: 13, 2: 9, 3: 2, 4: 4}))

    # U3: Button B opto
    footprints.append(dip4_opto("U3", "PC817", OX + 26, OY + 23, 0,
                                {1: 14, 2: 11, 3: 2, 4: 5}))

    # ── Pull-up resistors (10kΩ each, to VCC) ──
    # R4: Coin sense pull-up
    footprints.append(resistor_th("R4", "10K", OX + 35, OY + 7, 0, 1, 3))

    # R5: Button A pull-up
    footprints.append(resistor_th("R5", "10K", OX + 35, OY + 15, 0, 1, 4))

    # R6: Button B pull-up
    footprints.append(resistor_th("R6", "10K", OX + 35, OY + 23, 0, 1, 5))

    # ── Output header (right side) — connects to carrier board ──
    # J4: 6-pin header: COIN_SENSE, BTN_A, BTN_B, +3V3, GND, +5V
    # Pin order matches carrier board J_COIN: GPIO36, GPIO39, GPIO35, +3V3, GND, +5V
    footprints.append(pin_header_1xN(6, "J4", "TO_ESP32", OX + 42, OY + 15, 0,
                                     {1: 3, 2: 4, 3: 5, 4: 1, 5: 2, 6: 15}))
    labels.append(silk_text("COIN", OX + 42, OY + 8.0))
    labels.append(silk_text("A", OX + 42, OY + 10.5))
    labels.append(silk_text("B", OX + 42, OY + 13.0))
    labels.append(silk_text("3V3", OX + 42, OY + 15.5))
    labels.append(silk_text("GND", OX + 42, OY + 18.0))
    labels.append(silk_text("5V", OX + 42, OY + 20.5))

    # ── Mounting holes ──
    footprints.append(mounting_hole("MH1", OX + MOUNT_INSET, OY + MOUNT_INSET))
    footprints.append(mounting_hole("MH2", OX + BOARD_W - MOUNT_INSET, OY + MOUNT_INSET))
    footprints.append(mounting_hole("MH3", OX + MOUNT_INSET, OY + BOARD_H - MOUNT_INSET))
    footprints.append(mounting_hole("MH4", OX + BOARD_W - MOUNT_INSET, OY + BOARD_H - MOUNT_INSET))

    # ── Board title ──
    labels.append(silk_text("K6 GPO A+B Coin Box", OX + BOARD_W/2, OY + BOARD_H - 3, size=1.0, thickness=0.15))
    labels.append(silk_text("Daughter Board", OX + BOARD_W/2, OY + BOARD_H - 1, size=0.8, thickness=0.12))

    # ── Board outline ──
    edge = board_outline()

    # ── Ground fill zones (both layers) ──
    zones = ground_zones()

    return footprints, labels, edge, zones

def board_outline():
    """Edge.Cuts rectangle with rounded corners."""
    x1, y1 = OX, OY
    x2, y2 = OX + BOARD_W, OY + BOARD_H
    r = CORNER_R
    lines = []
    # Top edge
    lines.append(f'  (gr_line (start {x1+r} {y1}) (end {x2-r} {y1}) '
                 f'(stroke (width 0.05) (type default)) (layer "Edge.Cuts") (tstamp {uid()}))')
    # Bottom edge
    lines.append(f'  (gr_line (start {x1+r} {y2}) (end {x2-r} {y2}) '
                 f'(stroke (width 0.05) (type default)) (layer "Edge.Cuts") (tstamp {uid()}))')
    # Left edge
    lines.append(f'  (gr_line (start {x1} {y1+r}) (end {x1} {y2-r}) '
                 f'(stroke (width 0.05) (type default)) (layer "Edge.Cuts") (tstamp {uid()}))')
    # Right edge
    lines.append(f'  (gr_line (start {x2} {y1+r}) (end {x2} {y2-r}) '
                 f'(stroke (width 0.05) (type default)) (layer "Edge.Cuts") (tstamp {uid()}))')
    # Corner arcs
    corners = [
        (x1+r, y1+r, x1, y1+r, x1+r, y1),   # TL
        (x2-r, y1+r, x2-r, y1, x2, y1+r),    # TR
        (x1+r, y2-r, x1+r, y2, x1, y2-r),    # BL
        (x2-r, y2-r, x2, y2-r, x2-r, y2),    # BR
    ]
    for cx, cy, sx, sy, ex, ey in corners:
        lines.append(f'  (gr_arc (start {sx} {sy}) (mid {cx} {cy}) (end {ex} {ey}) '
                     f'(stroke (width 0.05) (type default)) (layer "Edge.Cuts") (tstamp {uid()}))')
    return "\n".join(lines)

def ground_zones():
    x1, y1 = OX + 0.5, OY + 0.5
    x2, y2 = OX + BOARD_W - 0.5, OY + BOARD_H - 0.5
    zones = []
    for layer in ("F.Cu", "B.Cu"):
        zones.append(
            f'  (zone (net 2) (net_name "GND") (layer "{layer}") (tstamp {uid()})\n'
            f'    (connect_pads (clearance 0.3))\n'
            f'    (fill yes (thermal_gap 0.5) (thermal_bridge_width 0.5))\n'
            f'    (polygon (pts\n'
            f'      (xy {x1} {y1}) (xy {x2} {y1}) (xy {x2} {y2}) (xy {x1} {y2})\n'
            f'    ))\n'
            f'  )'
        )
    return "\n".join(zones)

# ═════════════════════════════════════════════════════════════════════
#  OUTPUT FILES
# ═════════════════════════════════════════════════════════════════════

def write_pcb(footprints, labels, edge, zones):
    pcb = textwrap.dedent(f"""\
    (kicad_pcb (version 20260206) (generator "generate_coinbox_daughter.py")
      (general (thickness 1.6) (legacy_teardrops no))
      (paper "A4")
      (layers
        (0 "F.Cu" signal)
        (2 "B.Cu" signal)
        (32 "B.Adhes" user "B.Adhesive")
        (33 "F.Adhes" user "F.Adhesive")
        (34 "B.Paste" user)
        (35 "F.Paste" user)
        (36 "B.SilkS" user "B.Silkscreen")
        (37 "F.SilkS" user "F.Silkscreen")
        (38 "B.Mask" user)
        (39 "F.Mask" user)
        (40 "Dwgs.User" user "User.Drawings")
        (41 "Cmts.User" user "User.Comments")
        (42 "Eco1.User" user "User.Eco1")
        (43 "Eco2.User" user "User.Eco2")
        (44 "Edge.Cuts" user)
        (45 "Margin" user)
        (46 "B.CrtYd" user "B.Courtyard")
        (47 "F.CrtYd" user "F.Courtyard")
        (48 "B.Fab" user "B.Fabrication")
        (49 "F.Fab" user "F.Fabrication")
        (50 "User.1" user)
        (51 "User.2" user)
      )
      (setup
        (pad_to_mask_clearance 0.05)
        (allow_soldermask_bridges_in_footprints no)
        (pcbplotparams (layerselection 0x00010fc_ffffffff) (plot_on_all_layers_selection 0x0000000_00000000))
      )
    {net_defs()}

    {edge}

    {chr(10).join(footprints)}

    {chr(10).join(labels)}

    {zones}
    )
    """)
    with open("pcb/k6_coinbox_daughter.kicad_pcb", "w") as f:
        f.write(pcb)
    print("wrote pcb/k6_coinbox_daughter.kicad_pcb")

def write_project():
    proj = {
        "meta": {"filename": "k6_coinbox_daughter.kicad_pro", "version": 2},
        "board": {"design_settings": {"defaults": {"board_outline_line_width": 0.05}}},
        "schematic": {"meta": {"version": 1}},
    }
    with open("pcb/k6_coinbox_daughter.kicad_pro", "w") as f:
        json.dump(proj, f, indent=2)
    print("wrote pcb/k6_coinbox_daughter.kicad_pro")

def write_schematic():
    sch = textwrap.dedent("""\
    (kicad_sch (version 20231120) (generator "generate_coinbox_daughter.py")
      (paper "A4")
      (lib_symbols)
      (symbol_instances)
    )
    """)
    with open("pcb/k6_coinbox_daughter.kicad_sch", "w") as f:
        f.write(sch)
    print("wrote pcb/k6_coinbox_daughter.kicad_sch")

# ═════════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    footprints, labels, edge, zones = build_pcb()
    write_pcb(footprints, labels, edge, zones)
    write_project()
    write_schematic()
    print("\nDaughter board files generated successfully.")
    print(f"  Board size: {BOARD_W} x {BOARD_H} mm")
    print(f"  Components: 3x optocouplers, 3x 470Ω, 3x 10kΩ, 3x screw terminals, 1x 6-pin header")
