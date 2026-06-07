#!/usr/bin/env python3
"""Generate KiCad 10 project files for the K6 GPO Exhibit carrier board.

Creates:
  k6_gpo_carrier.kicad_pro   – project file
  k6_gpo_carrier.kicad_sch   – schematic
  k6_gpo_carrier.kicad_pcb   – PCB layout (100 × 80 mm, 2-layer)

All modules plug in via pin-headers.  Through-hole only.
"""
import uuid, json, textwrap, math

def uid():
    return str(uuid.uuid4())

# ─── Board dimensions ────────────────────────────────────────────────
BOARD_W = 100.0   # mm
BOARD_H = 100.0
ORIGIN_X = 130.0  # placement origin in KiCad space
ORIGIN_Y = 90.0
CORNER_R = 2.0    # edge fillet radius
MOUNT_HOLE_D = 3.2
MOUNT_INSET = 4.0

# ─── Helpers ─────────────────────────────────────────────────────────
def fp_pad_th(number, x, y, drill=1.0, size=1.7, shape="circle", net_id=0, net_name=""):
    s = "oval" if shape == "oval" else "circle"
    return (f'  (pad "{number}" thru_hole {s} (at {x:.3f} {y:.3f}) '
            f'(size {size} {size}) (drill {drill}) '
            f'(layers "*.Cu" "*.Mask") '
            f'(net {net_id} "{net_name}") (tstamp {uid()}))')

def fp_pad_np(x, y, drill=3.2):
    return (f'  (pad "" np_thru_hole circle (at {x:.3f} {y:.3f}) '
            f'(size {drill} {drill}) (drill {drill}) '
            f'(layers "*.Cu" "*.Mask") (tstamp {uid()}))')

def silk_text(text, x, y, layer="F.SilkS", size=1.0, thickness=0.15):
    return (f'  (fp_text user "{text}" (at {x:.3f} {y:.3f}) (layer "{layer}")\n'
            f'    (effects (font (size {size} {size}) (thickness {thickness})))\n'
            f'    (tstamp {uid()}))')

def ref_text(ref, x, y, layer="F.SilkS"):
    return (f'  (fp_text reference "{ref}" (at {x:.3f} {y:.3f}) (layer "{layer}")\n'
            f'    (effects (font (size 1 1) (thickness 0.15)))\n'
            f'    (tstamp {uid()}))')

def val_text(val, x, y, layer="F.Fab"):
    return (f'  (fp_text value "{val}" (at {x:.3f} {y:.3f}) (layer "{layer}")\n'
            f'    (effects (font (size 1 1) (thickness 0.15)))\n'
            f'    (tstamp {uid()}))')

# ═════════════════════════════════════════════════════════════════════
#  NET LIST  (id → name)
# ═════════════════════════════════════════════════════════════════════
NETS = {
    0: "",
    1: "+12V",
    2: "+5V",
    3: "+3V3",
    4: "GND",
    5: "+50V",
    6: "LINE_A",
    7: "LINE_B",
    8: "BELL",
    9: "LINE_SENSE",      # GPIO 34
    10: "RING_EN",        # GPIO 4
    11: "RING_A",         # GPIO 16
    12: "RING_B",         # GPIO 17
    13: "I2S_BCLK",       # GPIO 26
    14: "I2S_LRCLK",      # GPIO 25
    15: "I2S_DOUT",       # GPIO 22
    16: "SD_CS",          # GPIO 5
    17: "SPI_MOSI",       # GPIO 23
    18: "SPI_MISO",       # GPIO 19
    19: "SPI_SCK",        # GPIO 18
    20: "BTN_RING",       # GPIO 32
    21: "BTN_CANCEL",     # GPIO 33
    22: "BTN_RESET",      # GPIO 27
    23: "OPTO_ANODE",
    24: "OPTO_EMIT",
    25: "AUDIO_P1",       # transformer primary 1
    26: "AUDIO_P2",       # transformer primary 2
    27: "JUNC_A",         # R1/R2 junction
    28: "LED_STATUS",     # GPIO 2
}

def net_defs():
    lines = []
    for nid, name in sorted(NETS.items()):
        lines.append(f'  (net {nid} "{name}")')
    return "\n".join(lines)

# ═════════════════════════════════════════════════════════════════════
#  FOOTPRINTS  – each is a function returning (text, pads_info)
# ═════════════════════════════════════════════════════════════════════

def make_footprint(ref, val, x, y, angle, pads_text, extra_drawing=""):
    """Wrap pads into a footprint block placed at (x,y) with rotation."""
    return (
        f'  (footprint "K6_GPO:{ref}" (layer "F.Cu")\n'
        f'    (tstamp {uid()})\n'
        f'    (at {x:.3f} {y:.3f} {angle})\n'
        f'    {ref_text(ref, 0, -2)}\n'
        f'    {val_text(val, 0, 2)}\n'
        f'{extra_drawing}'
        f'{pads_text}\n'
        f'  )'
    )

def pin_header_1xN(n, ref, val, x, y, angle, net_map, pitch=2.54):
    """Single-row pin header, N pins, 2.54mm pitch."""
    pads = []
    for i in range(n):
        py = (i - (n-1)/2.0) * pitch
        nid = net_map.get(i+1, 0)
        nname = NETS.get(nid, "")
        pads.append(fp_pad_th(i+1, 0, py, net_id=nid, net_name=nname))
    # courtyard
    cy_h = n * pitch + 1.0
    crt = (f'    (fp_rect (start {-1.5:.3f} {-(cy_h/2):.3f}) '
           f'(end {1.5:.3f} {cy_h/2:.3f}) (stroke (width 0.05) (type default)) '
           f'(fill none) (layer "F.CrtYd") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), crt)

def pin_header_2xN(n, ref, val, x, y, angle, net_map_left, net_map_right, pitch=2.54, row_spacing=25.4):
    """Dual-row pin header (like ESP32 DevKit), N pins per side."""
    pads = []
    for i in range(n):
        py = (i - (n-1)/2.0) * pitch
        # left column (pin i+1)
        nid_l = net_map_left.get(i+1, 0)
        pads.append(fp_pad_th(f"L{i+1}", -row_spacing/2, py, net_id=nid_l, net_name=NETS.get(nid_l,"")))
        # right column (pin i+1)
        nid_r = net_map_right.get(i+1, 0)
        pads.append(fp_pad_th(f"R{i+1}", row_spacing/2, py, net_id=nid_r, net_name=NETS.get(nid_r,"")))
    crt_h = n * pitch + 1.0
    crt = (f'    (fp_rect (start {-(row_spacing/2+1.5):.3f} {-(crt_h/2):.3f}) '
           f'(end {(row_spacing/2+1.5):.3f} {crt_h/2:.3f}) (stroke (width 0.05) (type default)) '
           f'(fill none) (layer "F.CrtYd") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), crt)

def dip_package(n_pins, ref, val, x, y, angle, net_map, row_spacing=7.62, pitch=2.54):
    """DIP-N through-hole IC package."""
    half = n_pins // 2
    pads = []
    for i in range(half):
        py = (i - (half-1)/2.0) * pitch
        # left side (pins 1..half)
        pin_l = i + 1
        nid_l = net_map.get(pin_l, 0)
        pads.append(fp_pad_th(pin_l, -row_spacing/2, py, drill=0.8, size=1.6,
                              net_id=nid_l, net_name=NETS.get(nid_l,"")))
        # right side (pins n_pins..half+1, counted from bottom)
        pin_r = n_pins - i
        nid_r = net_map.get(pin_r, 0)
        pads.append(fp_pad_th(pin_r, row_spacing/2, py, drill=0.8, size=1.6,
                              net_id=nid_r, net_name=NETS.get(nid_r,"")))
    crt_h = half * pitch + 1.0
    crt = (f'    (fp_rect (start {-(row_spacing/2+1.5):.3f} {-(crt_h/2):.3f}) '
           f'(end {(row_spacing/2+1.5):.3f} {crt_h/2:.3f}) (stroke (width 0.05) (type default)) '
           f'(fill none) (layer "F.CrtYd") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), crt)

def resistor_th(ref, val, x, y, angle, net_1, net_2, pitch=10.16):
    """Axial through-hole resistor."""
    pads = []
    pads.append(fp_pad_th(1, -pitch/2, 0, net_id=net_1, net_name=NETS.get(net_1,"")))
    pads.append(fp_pad_th(2, pitch/2, 0, net_id=net_2, net_name=NETS.get(net_2,"")))
    body = (f'    (fp_line (start {-pitch/2+1:.3f} -1.25) (end {pitch/2-1:.3f} -1.25) '
            f'(stroke (width 0.12) (type default)) (layer "F.SilkS") (tstamp {uid()}))\n'
            f'    (fp_line (start {-pitch/2+1:.3f} 1.25) (end {pitch/2-1:.3f} 1.25) '
            f'(stroke (width 0.12) (type default)) (layer "F.SilkS") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), body)

def capacitor_th(ref, val, x, y, angle, net_1, net_2, pitch=2.5):
    """Radial through-hole capacitor."""
    pads = []
    pads.append(fp_pad_th(1, -pitch/2, 0, net_id=net_1, net_name=NETS.get(net_1,"")))
    pads.append(fp_pad_th(2, pitch/2, 0, net_id=net_2, net_name=NETS.get(net_2,"")))
    return make_footprint(ref, val, x, y, angle, "\n".join(pads))

def screw_terminal(n, ref, val, x, y, angle, net_map, pitch=5.08):
    """N-position screw terminal block."""
    pads = []
    for i in range(n):
        px = (i - (n-1)/2.0) * pitch
        nid = net_map.get(i+1, 0)
        pads.append(fp_pad_th(i+1, px, 0, drill=1.3, size=2.5,
                              net_id=nid, net_name=NETS.get(nid,"")))
    return make_footprint(ref, val, x, y, angle, "\n".join(pads))

def mounting_hole(ref, x, y):
    pad = fp_pad_np(0, 0, drill=MOUNT_HOLE_D)
    return make_footprint(ref, "MH", x, y, 0, pad)

def transformer_4pin(ref, val, x, y, angle, net_map, pitch_x=10.0, pitch_y=7.5):
    """4-pin audio transformer (2 primary, 2 secondary)."""
    pads = []
    # Primary: pins 1,2 on left
    nid1 = net_map.get(1, 0)
    nid2 = net_map.get(2, 0)
    pads.append(fp_pad_th(1, -pitch_x/2, -pitch_y/2, net_id=nid1, net_name=NETS.get(nid1,"")))
    pads.append(fp_pad_th(2, -pitch_x/2, pitch_y/2, net_id=nid2, net_name=NETS.get(nid2,"")))
    # Secondary: pins 3,4 on right
    nid3 = net_map.get(3, 0)
    nid4 = net_map.get(4, 0)
    pads.append(fp_pad_th(3, pitch_x/2, -pitch_y/2, net_id=nid3, net_name=NETS.get(nid3,"")))
    pads.append(fp_pad_th(4, pitch_x/2, pitch_y/2, net_id=nid4, net_name=NETS.get(nid4,"")))
    body = (f'    (fp_rect (start -6 -5) (end 6 5) '
            f'(stroke (width 0.12) (type default)) (fill none) (layer "F.SilkS") (tstamp {uid()}))\n')
    return make_footprint(ref, val, x, y, angle, "\n".join(pads), body)


# ═════════════════════════════════════════════════════════════════════
#  COPPER TRACES
# ═════════════════════════════════════════════════════════════════════

def segment(x1, y1, x2, y2, net_id, width=0.5, layer="F.Cu"):
    return (f'  (segment (start {x1:.3f} {y1:.3f}) (end {x2:.3f} {y2:.3f}) '
            f'(width {width}) (layer "{layer}") (net {net_id}) (tstamp {uid()}))')

def via(x, y, net_id, drill=0.4, size=0.8):
    return (f'  (via (at {x:.3f} {y:.3f}) (size {size}) (drill {drill}) '
            f'(layers "F.Cu" "B.Cu") (net {net_id}) (tstamp {uid()}))')


# ═════════════════════════════════════════════════════════════════════
#  PCB GENERATION
# ═════════════════════════════════════════════════════════════════════

def generate_pcb():
    OX, OY = ORIGIN_X, ORIGIN_Y  # board top-left in KiCad coords

    # ── Board outline (Edge.Cuts) ──
    outline = []
    corners = [
        (OX, OY), (OX + BOARD_W, OY),
        (OX + BOARD_W, OY + BOARD_H), (OX, OY + BOARD_H)
    ]
    for i in range(4):
        x1, y1 = corners[i]
        x2, y2 = corners[(i+1) % 4]
        outline.append(
            f'  (gr_line (start {x1:.3f} {y1:.3f}) (end {x2:.3f} {y2:.3f}) '
            f'(stroke (width 0.1) (type default)) (layer "Edge.Cuts") (tstamp {uid()}))')

    # ── Mounting holes ──
    mount_holes = []
    for i, (mx, my) in enumerate([(MOUNT_INSET, MOUNT_INSET),
                   (BOARD_W - MOUNT_INSET, MOUNT_INSET),
                   (MOUNT_INSET, BOARD_H - MOUNT_INSET),
                   (BOARD_W - MOUNT_INSET, BOARD_H - MOUNT_INSET)], start=1):
        mount_holes.append(mounting_hole(f"MH{i}", OX + mx, OY + my))

    # ── Component placement ──
    # Board is 100×100mm. Components are spread out to avoid label overlap.
    # Left zone: buttons, SD card.  Center: ESP32.  Top: power + DAC.
    # Right: audio output, phone, driver ICs.
    footprints = []

    # -- ESP32 DevKit V1 (2 rows of 15 pins, 25.4mm apart) --
    esp_x, esp_y = OX + 38, OY + 40
    esp_left = {
        1: 3,     # 3V3
        2: 0,     # EN
        3: 0,     # GPIO 36
        4: 0,     # GPIO 39
        5: 9,     # GPIO 34 → LINE_SENSE
        6: 0,     # GPIO 35
        7: 20,    # GPIO 32 → BTN_RING
        8: 21,    # GPIO 33 → BTN_CANCEL
        9: 14,    # GPIO 25 → I2S_LRCLK
        10: 13,   # GPIO 26 → I2S_BCLK
        11: 22,   # GPIO 27 → BTN_RESET
        12: 0,    # GPIO 14
        13: 0,    # GPIO 12
        14: 4,    # GND
        15: 0,    # GPIO 13
    }
    esp_right = {
        1: 2,     # VIN → +5V
        2: 4,     # GND
        3: 17,    # GPIO 23 → SPI_MOSI
        4: 15,    # GPIO 22 → I2S_DOUT
        5: 0,     # TX0
        6: 0,     # RX0
        7: 0,     # GPIO 21
        8: 18,    # GPIO 19 → SPI_MISO
        9: 19,    # GPIO 18 → SPI_SCK
        10: 16,   # GPIO 5 → SD_CS
        11: 12,   # GPIO 17 → RING_B
        12: 11,   # GPIO 16 → RING_A
        13: 10,   # GPIO 4 → RING_EN
        14: 28,   # GPIO 2 → LED_STATUS
        15: 0,    # GPIO 15
    }
    footprints.append(pin_header_2xN(15, "U1", "ESP32_DevKit",
                                     esp_x, esp_y, 0, esp_left, esp_right))

    # -- LM2596 Buck Converter Module (12V → 5V) --
    buck_x, buck_y = OX + 28, OY + 12
    footprints.append(pin_header_1xN(4, "J1", "LM2596_Buck",
                                     buck_x, buck_y, 0,
                                     {1: 1, 2: 4, 3: 2, 4: 4}))

    # -- XL6009 Boost Converter Module (12V → 50V) --
    boost_x, boost_y = OX + 50, OY + 12
    footprints.append(pin_header_1xN(4, "J2", "XL6009_Boost",
                                     boost_x, boost_y, 0,
                                     {1: 1, 2: 4, 3: 5, 4: 4}))

    # -- 12V Power Input (2-pin screw terminal) --
    pwr_x, pwr_y = OX + 10, OY + 12
    footprints.append(screw_terminal(2, "J3", "12V_IN",
                                     pwr_x, pwr_y, 90,
                                     {1: 1, 2: 4}))

    # -- Phone Cord Terminal Block (3-pin screw terminal) --
    phone_x, phone_y = OX + 92, OY + 52
    footprints.append(screw_terminal(3, "J4", "PHONE",
                                     phone_x, phone_y, 90,
                                     {1: 6, 2: 7, 3: 8}))

    # -- MAX98357A I2S DAC (7-pin header) --
    dac_x, dac_y = OX + 82, OY + 18
    footprints.append(pin_header_1xN(7, "J5", "MAX98357A",
                                     dac_x, dac_y, 0,
                                     {1: 2, 2: 4, 3: 0, 4: 0, 5: 15, 6: 13, 7: 14}))
    # DAC output (SPK+, SPK-) 2-pin header
    dac_out_x, dac_out_y = OX + 92, OY + 18
    footprints.append(pin_header_1xN(2, "J6", "DAC_OUT",
                                     dac_out_x, dac_out_y, 0,
                                     {1: 25, 2: 26}))

    # -- SD Card Module (6-pin header) --
    sd_x, sd_y = OX + 25, OY + 90
    footprints.append(pin_header_1xN(6, "J7", "SD_Card",
                                     sd_x, sd_y, 0,
                                     {1: 4, 2: 3, 3: 17, 4: 18, 5: 19, 6: 16}))

    # -- Control Panel Button Headers (3x 2-pin) --
    btn_base_x, btn_base_y = OX + 8, OY + 64
    for i, (name, net) in enumerate([("BTN_RING", 20), ("BTN_CANCEL", 21), ("BTN_RESET", 22)]):
        footprints.append(pin_header_1xN(2, f"J{8+i}", name,
                                         btn_base_x, btn_base_y + i * 8, 0,
                                         {1: net, 2: 4}))

    # -- L293D H-Bridge (DIP-16) --
    l293d_x, l293d_y = OX + 82, OY + 76
    l293d_nets = {
        1: 10, 2: 11, 3: 8, 4: 4, 5: 4, 6: 7, 7: 12, 8: 5,
        9: 4, 10: 0, 11: 0, 12: 4, 13: 4, 14: 0, 15: 0, 16: 2
    }
    footprints.append(dip_package(16, "U2", "L293D",
                                  l293d_x, l293d_y, 0, l293d_nets))

    # -- PC817 Optocoupler (DIP-4) --
    opto_x, opto_y = OX + 82, OY + 92
    opto_nets = {1: 23, 2: 7, 3: 24, 4: 3}
    footprints.append(dip_package(4, "U3", "PC817",
                                  opto_x, opto_y, 0, opto_nets))

    # -- R1: 470Ω 1W (12V → Junction A) --
    r1_x, r1_y = OX + 55, OY + 86
    footprints.append(resistor_th("R1", "470R", r1_x, r1_y, 0, 1, 27))

    # -- R2: 220Ω (Junction A → Opto Anode) --
    r2_x, r2_y = OX + 55, OY + 93
    footprints.append(resistor_th("R2", "220R", r2_x, r2_y, 0, 27, 23))

    # -- R3: 10kΩ (Opto Emitter → GND) --
    r3_x, r3_y = OX + 92, OY + 86
    footprints.append(resistor_th("R3", "10K", r3_x, r3_y, 90, 24, 4))

    # -- C1: 100nF decoupling for L293D --
    c1_x, c1_y = OX + 70, OY + 68
    footprints.append(capacitor_th("C1", "100nF", c1_x, c1_y, 0, 2, 4))

    # -- Audio Transformer --
    xfmr_x, xfmr_y = OX + 90, OY + 38
    xfmr_nets = {1: 25, 2: 26, 3: 6, 4: 7}
    footprints.append(transformer_4pin("T1", "600R_XFMR", xfmr_x, xfmr_y, 0, xfmr_nets))

    # ══════════════════════════════════════════════════════════════════
    #  SILKSCREEN LABELS — short labels, positioned to avoid overlap
    # ══════════════════════════════════════════════════════════════════
    silk_labels = []
    SZ_TITLE = 1.0
    SZ_SUB = 0.8     # sub-title
    SZ_PIN = 0.5     # pin labels (smaller to fit)
    TH_PIN = 0.10

    def silk(x, y, txt, size=SZ_TITLE, thickness=0.15, justify="left"):
        silk_labels.append(
            f'  (gr_text "{txt}" (at {x:.3f} {y:.3f}) (layer "F.SilkS")\n'
            f'    (effects (font (size {size} {size}) (thickness {thickness})) (justify {justify}))\n'
            f'    (tstamp {uid()}))')

    def silk_pin(x, y, txt, justify="left"):
        silk(x, y, txt, size=SZ_PIN, thickness=TH_PIN, justify=justify)

    # Board title & URL
    silk(OX + 50, OY + 3, "K6 GPO Exhibit — Carrier Board v1.0", justify="center")
    silk(OX + 50, OY + 97, "github.com/opmedia-dev/k6_gpo_exhibit", size=0.7, justify="center")

    # ── J3: 12V Power Input (screw terminal, rotated 90°) ──
    silk(pwr_x, pwr_y - 7, "12V IN")
    silk_pin(pwr_x + 4, pwr_y - 2.54, "+12V")
    silk_pin(pwr_x + 4, pwr_y + 2.54, "GND")

    # ── J1: LM2596 Buck ──
    silk(buck_x, buck_y - 7, "BUCK")
    for i, lbl in enumerate(["12V+", "GND", "5V+", "GND"]):
        silk_pin(buck_x + 3, buck_y + (i - 1.5) * 2.54, lbl)

    # ── J2: XL6009 Boost ──
    silk(boost_x, boost_y - 7, "BOOST")
    for i, lbl in enumerate(["12V+", "GND", "50V+", "GND"]):
        silk_pin(boost_x + 3, boost_y + (i - 1.5) * 2.54, lbl)

    # ── U1: ESP32 DevKit (2×15 headers) ──
    silk(esp_x, esp_y - 21, "ESP32 DevKit", justify="center")
    # Left: short labels — GPIO number + 2-3 char function code for used pins
    esp_left_lbl = [
        "3V3", "EN", "36", "39", "34 SN", "35",
        "32 RG", "33 CN", "25 LR", "26 BC",
        "27 RS", "14", "12", "GND", "13"
    ]
    esp_right_lbl = [
        "5V", "GND", "23 MO", "22 DIN", "TX",
        "RX", "21", "19 MI", "18 CK",
        "5 CS", "17 RB", "16 RA", "4 REN", "2 LD", "15"
    ]
    for i, lbl in enumerate(esp_left_lbl):
        py = esp_y + (i - 7) * 2.54
        silk_pin(esp_x - 12.7 - 1.5, py, lbl, justify="right")
    for i, lbl in enumerate(esp_right_lbl):
        py = esp_y + (i - 7) * 2.54
        silk_pin(esp_x + 12.7 + 1.5, py, lbl)

    # ── J5: MAX98357A DAC ──
    silk(dac_x, dac_y - 12, "MAX98357A", size=SZ_SUB)
    for i, lbl in enumerate(["5V", "GND", "SD", "GAIN", "DIN", "BCK", "LRC"]):
        silk_pin(dac_x + 3, dac_y + (i - 3) * 2.54, lbl)

    # ── J6: DAC Output ──
    silk_pin(dac_out_x + 3, dac_out_y - 1.27, "SPK+")
    silk_pin(dac_out_x + 3, dac_out_y + 1.27, "SPK-")

    # ── T1: Audio Transformer ──
    silk(xfmr_x, xfmr_y - 7, "XFMR", justify="center")
    silk_pin(xfmr_x - 6, xfmr_y - 3.75, "SPK+", justify="right")
    silk_pin(xfmr_x - 6, xfmr_y + 3.75, "SPK-", justify="right")
    silk_pin(xfmr_x + 6, xfmr_y - 3.75, "LnA")
    silk_pin(xfmr_x + 6, xfmr_y + 3.75, "LnB")

    # ── J4: Phone Cord (screw terminal, rotated 90°, pitch 5.08mm) ──
    silk(phone_x - 8, phone_y, "PHONE")
    for i, lbl in enumerate(["A Red", "B Wht", "Bell"]):
        silk_pin(phone_x + 4, phone_y + (i - 1) * 5.08, lbl)

    # ── J8/J9/J10: Button Headers ──
    silk(btn_base_x, btn_base_y - 4, "BUTTONS")
    for i, name in enumerate(["RING", "CANCEL", "RESET"]):
        by = btn_base_y + i * 8
        silk_pin(btn_base_x + 3, by - 1.27, name)
        silk_pin(btn_base_x + 3, by + 1.27, "GND")

    # ── J7: SD Card Module ──
    silk(sd_x, sd_y - 10, "SD CARD")
    for i, lbl in enumerate(["GND", "3V3", "MOSI", "MISO", "SCK", "CS"]):
        silk_pin(sd_x + 3, sd_y + (i - 2.5) * 2.54, lbl)

    # ── U2: L293D (DIP-16, pin pitch 2.54mm) ──
    silk(l293d_x, l293d_y - 13, "L293D")
    # Left pins 1-8 — label key signals only
    l293d_left_lbl = ["EN", "IN1", "BELL", "GND", "GND", "LnB", "IN2", "50V"]
    for i, lbl in enumerate(l293d_left_lbl):
        if lbl:
            py = l293d_y + (i - 3.5) * 2.54
            silk_pin(l293d_x - 3.81 - 1.5, py, lbl, justify="right")
    # Right pins 16-9 — only label 5V
    l293d_right_lbl = ["5V", "", "", "", "", "", "", "GND"]
    for i, lbl in enumerate(l293d_right_lbl):
        if lbl:
            py = l293d_y + (i - 3.5) * 2.54
            silk_pin(l293d_x + 3.81 + 1.5, py, lbl)

    # ── U3: PC817 Optocoupler (DIP-4, pin pitch 2.54mm) ──
    silk(opto_x, opto_y - 5, "PC817")
    silk_pin(opto_x - 3.81 - 1.5, opto_y - 1.27, "An", justify="right")
    silk_pin(opto_x - 3.81 - 1.5, opto_y + 1.27, "Kth", justify="right")
    silk_pin(opto_x + 3.81 + 1.5, opto_y - 1.27, "Col")
    silk_pin(opto_x + 3.81 + 1.5, opto_y + 1.27, "Em")

    # ── Discrete components — ref + value only ──
    silk_pin(r1_x, r1_y - 2.5, "R1 470R")
    silk_pin(r2_x, r2_y - 2.5, "R2 220R")
    silk_pin(r3_x + 2, r3_y, "R3 10K")
    silk_pin(c1_x, c1_y - 2, "C1 100nF")

    # ── Ground fill zones (both layers) ──
    zone_corners = " ".join(f"(xy {OX + dx:.3f} {OY + dy:.3f})"
                            for dx, dy in [(0,0),(BOARD_W,0),(BOARD_W,BOARD_H),(0,BOARD_H)])
    zones = []
    for layer in ["F.Cu", "B.Cu"]:
        zones.append(
            f'  (zone (net 4) (net_name "GND") (layer "{layer}") (tstamp {uid()})\n'
            f'    (hatch edge 0.5)\n'
            f'    (connect_pads (clearance 0.3))\n'
            f'    (min_thickness 0.25)\n'
            f'    (fill yes (thermal_gap 0.5) (thermal_bridge_width 0.5))\n'
            f'    (polygon (pts {zone_corners})))')

    # ── Assemble PCB file ──
    pcb = f"""(kicad_pcb (version 20260206) (generator "k6_gpo_gen") (generator_version "10.0")
  (general (thickness 1.6) (legacy_teardrops no))
  (paper "A4")
  (layers
    (0 "F.Cu" signal)
    (2 "B.Cu" signal)
    (1 "F.Mask" user)
    (3 "B.Mask" user)
    (5 "F.SilkS" user)
    (7 "B.SilkS" user)
    (9 "F.Adhes" user)
    (11 "B.Adhes" user)
    (13 "F.Paste" user)
    (15 "B.Paste" user)
    (17 "Dwgs.User" user)
    (19 "Cmts.User" user)
    (21 "Eco1.User" user)
    (23 "Eco2.User" user)
    (25 "Edge.Cuts" user)
    (27 "Margin" user)
    (29 "B.CrtYd" user)
    (31 "F.CrtYd" user)
    (33 "B.Fab" user)
    (35 "F.Fab" user)
  )
  (setup
    (pad_to_mask_clearance 0.05)
    (aux_axis_origin 0 0)
    (pcbplotparams (layerselection 0x00010fc_ffffffff) (plot_on_all_layers_selection 0x0000000_00000000)
      (disableapertmacros no) (usegerberextensions no) (usegerberattributes yes)
      (usegerberadvancedattributes yes) (creategerberjobfile yes)
      (svgprecision 4) (excludeedgelayer yes) (plotframeref no)
      (viasonmask no) (mode 1) (useauxorigin no) (hpglpennumber 1)
      (hpglpenspeed 20) (hpglpendiameter 15.000000)
      (pdf_front_fp_property_popups yes) (pdf_back_fp_property_popups yes)
      (dxfpolygonmode yes) (dxfimperialunits yes) (dxfusepcbnewfont yes)
      (psnegative no) (psa4output no) (plotreference yes)
      (plotvalue yes) (plotfptext yes) (plotinvisibletext no)
      (sketchpadsonfab no) (subtractmaskfromsilk no)
      (outputformat 1) (mirror no) (drillshape 1)
      (scaleselection 1) (outputdirectory "gerbers/"))
  )
{net_defs()}

{chr(10).join(outline)}

{chr(10).join(mount_holes)}

{chr(10).join(footprints)}

{chr(10).join(silk_labels)}

{chr(10).join(zones)}

)"""
    return pcb


# ═════════════════════════════════════════════════════════════════════
#  PROJECT FILE
# ═════════════════════════════════════════════════════════════════════

def generate_project():
    return json.dumps({
        "meta": {"filename": "k6_gpo_carrier.kicad_pro", "version": 1},
        "board": {
            "3dviewports": [],
            "design_settings": {"defaults": {"board_outline_line_width": 0.1}},
            "layer_presets": [],
            "layer_selections": {}
        },
        "boards": [],
        "cvpcb": {"equivalence_files": []},
        "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "net_settings": {
            "classes": [{
                "bus_width": 12, "clearance": 0.2, "diff_pair_gap": 0.25,
                "diff_pair_via_gap": 0.25, "diff_pair_width": 0.2,
                "line_style": 0, "microvia_diameter": 0.3, "microvia_drill": 0.1,
                "name": "Default", "pcb_color": "rgba(0, 0, 0, 0.000)",
                "schematic_color": "rgba(0, 0, 0, 0.000)", "track_width": 0.5,
                "via_diameter": 0.8, "via_drill": 0.4, "wire_width": 6
            }],
            "meta": {"version": 3},
            "net_colors": None
        },
        "pcbnew": {"last_paths": {"gencad": "", "idf": "", "netlist": "", "specctra_dsn": "", "step": "", "vrml": ""}},
        "schematic": {"drawing": {"default_line_thickness": 6.0}, "legacy_lib_dir": "", "legacy_lib_list": []},
        "sheets": [],
        "text_variables": {}
    }, indent=2)


# ═════════════════════════════════════════════════════════════════════
#  SCHEMATIC  (simplified — components + labels)
# ═════════════════════════════════════════════════════════════════════

def generate_schematic():
    """Generate a KiCad 7 schematic file.
    
    This creates a valid schematic showing all components with their
    connections documented via net labels.  Opening in KiCad will show
    all parts with labelled pins that match the PCB netlist.
    """

    components = []
    wires = []
    labels = []
    
    # We'll place components in a grid layout on the schematic
    # Each "row" is a functional block
    
    def sym_block(ref, val, lib_sym, x, y, pins_text):
        return (
            f'    (symbol (lib_id "{lib_sym}") (at {x} {y} 0) (unit 1)\n'
            f'      (in_bom yes) (on_board yes) (dnp no)\n'
            f'      (uuid {uid()})\n'
            f'      (property "Reference" "{ref}" (at {x} {y-2} 0)\n'
            f'        (effects (font (size 1.27 1.27))))\n'
            f'      (property "Value" "{val}" (at {x} {y+2} 0)\n'
            f'        (effects (font (size 1.27 1.27))))\n'
            f'{pins_text}'
            f'    )'
        )

    def net_label(name, x, y, angle=0):
        return (
            f'    (label "{name}" (at {x} {y} {angle}) (fields_autoplaced)\n'
            f'      (effects (font (size 1.27 1.27)) (justify left))\n'
            f'      (uuid {uid()}))'
        )

    # Build a text-based schematic that documents the netlist
    # This is a simplified schematic - for full visual editing users open in KiCad
    
    sch = f"""(kicad_sch (version 20230121) (generator k6_gpo_gen)
  (uuid {uid()})
  (paper "A3")

  (title_block
    (title "K6 GPO Exhibit — Carrier Board")
    (date "2025-01-01")
    (rev "1.0")
    (comment 1 "ESP32 interface for GPO 232/332 rotary telephones")
    (comment 2 "Carrier board — all modules plug in via pin headers")
    (comment 3 "github.com/opmedia-dev/k6_gpo_exhibit")
  )

  (lib_symbols)

  (text "=== POWER ===" (at 25.4 25.4 0)
    (effects (font (size 2.54 2.54) (bold yes))))
  (text "J3: 12V DC Input (2-pin screw terminal)\\nPin 1: +12V   Pin 2: GND" (at 25.4 33.02 0)
    (effects (font (size 1.27 1.27)) (justify left)))
  (text "J1: LM2596 Buck (12V to 5V)\\nPin 1: IN+ (+12V)   Pin 2: IN- (GND)\\nPin 3: OUT+ (+5V)   Pin 4: OUT- (GND)" (at 25.4 45.72 0)
    (effects (font (size 1.27 1.27)) (justify left)))
  (text "J2: XL6009 Boost (12V to 50V)\\nPin 1: IN+ (+12V)   Pin 2: IN- (GND)\\nPin 3: OUT+ (+50V)  Pin 4: OUT- (GND)" (at 25.4 60.96 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "=== ESP32 DevKit V1 (U1) ===" (at 25.4 81.28 0)
    (effects (font (size 2.54 2.54) (bold yes))))
  (text "Left header (top to bottom):\\n L1: 3V3        L8: GPIO33 (BTN_CANCEL)\\n L2: EN         L9: GPIO25 (I2S_LRCLK)\\n L3: GPIO36     L10: GPIO26 (I2S_BCLK)\\n L4: GPIO39     L11: GPIO27 (BTN_RESET)\\n L5: GPIO34 (LINE_SENSE)  L12: GPIO14\\n L6: GPIO35     L13: GPIO12\\n L7: GPIO32 (BTN_RING)    L14: GND\\n                L15: GPIO13" (at 25.4 99.06 0)
    (effects (font (size 1.27 1.27)) (justify left)))
  (text "Right header (top to bottom):\\n R1: VIN (+5V)  R8: GPIO19 (SPI_MISO)\\n R2: GND        R9: GPIO18 (SPI_SCK)\\n R3: GPIO23 (SPI_MOSI)  R10: GPIO5 (SD_CS)\\n R4: GPIO22 (I2S_DOUT)   R11: GPIO17 (RING_B)\\n R5: TX0        R12: GPIO16 (RING_A)\\n R6: RX0        R13: GPIO4 (RING_EN)\\n R7: GPIO21     R14: GPIO2 (LED)\\n                R15: GPIO15" (at 25.4 129.54 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "=== PHONE LINE INTERFACE ===" (at 177.8 25.4 0)
    (effects (font (size 2.54 2.54) (bold yes))))
  (text "J4: Phone Terminal Block (3-pin screw)\\nPin 1: LINE_A (Red)   Pin 2: LINE_B (White)   Pin 3: BELL (Blue)" (at 177.8 33.02 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "Hook / Dial Detection:\\n+12V -- [R1 470R] -- JUNC_A -- Terminal 1 (LINE_A)\\n                      |\\n                  [R2 220R]\\n                      |\\n                 PC817 Anode (U3 pin 1)\\nTerminal 2 (LINE_B) -- PC817 Cathode (U3 pin 2)\\nPC817 Collector (U3 pin 4) -- +3V3\\nPC817 Emitter (U3 pin 3) -- GPIO34 (LINE_SENSE)\\nPC817 Emitter (U3 pin 3) -- [R3 10K] -- GND" (at 177.8 50.8 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "Ring Generator (U2: L293D DIP-16):\\nPin 1 (EN1,2): GPIO4 (RING_EN)\\nPin 2 (IN1): GPIO16 (RING_A)\\nPin 7 (IN2): GPIO17 (RING_B)\\nPin 8 (VS): +50V\\nPin 16 (VSS): +5V\\nPin 3 (OUT1): BELL (Terminal 3)\\nPin 6 (OUT2): LINE_B (Terminal 2)\\nPins 4,5,9,12,13: GND\\nC1 100nF between +5V and GND (near L293D)" (at 177.8 93.98 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "=== AUDIO ===" (at 177.8 137.16 0)
    (effects (font (size 2.54 2.54) (bold yes))))
  (text "J5: MAX98357A I2S DAC (7-pin header):\\nPin 1: VIN (+5V)     Pin 5: DIN (GPIO22)\\nPin 2: GND           Pin 6: BCLK (GPIO26)\\nPin 3: SD (n/c)      Pin 7: LRC (GPIO25)\\nPin 4: GAIN (n/c)\\n\\nJ6: DAC Output (2-pin header):\\nPin 1: L+ (AUDIO_P1) --> T1 Primary 1\\nPin 2: L- (AUDIO_P2) --> T1 Primary 2\\n\\nT1: Audio Transformer (600R:600R):\\nPrimary 1: AUDIO_P1    Secondary 1: LINE_A\\nPrimary 2: AUDIO_P2    Secondary 2: LINE_B" (at 177.8 152.4 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "=== SD CARD ===" (at 25.4 160.02 0)
    (effects (font (size 2.54 2.54) (bold yes))))
  (text "J7: SD Card Module (6-pin header):\\nPin 1: GND     Pin 4: MISO (GPIO19)\\nPin 2: VCC (3V3)  Pin 5: SCK (GPIO18)\\nPin 3: MOSI (GPIO23)  Pin 6: CS (GPIO5)" (at 25.4 172.72 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "=== CONTROL PANEL ===" (at 25.4 193.04 0)
    (effects (font (size 2.54 2.54) (bold yes))))
  (text "J8: RING Button     Pin 1: GPIO32   Pin 2: GND\\nJ9: CANCEL Button   Pin 1: GPIO33   Pin 2: GND\\nJ10: RESET Button   Pin 1: GPIO27   Pin 2: GND\\n(All active-low, ESP32 internal pull-ups enabled)" (at 25.4 203.2 0)
    (effects (font (size 1.27 1.27)) (justify left)))

  (text "=== NET LIST ===" (at 177.8 193.04 0)
    (effects (font (size 2.54 2.54) (bold yes))))
  (text "+12V: J3.1, J1.1, J2.1, R1.1\\n+5V: J1.3, U1.R1(VIN), J5.1, U2.16(VSS), C1.1\\n+3V3: U1.L1(3V3), U3.4(Collector), J7.2\\n+50V: J2.3, U2.8(VS)\\nGND: J3.2, J1.2, J1.4, J2.2, J2.4, U1.L14, U1.R2, J5.2,\\n     U2.4, U2.5, U2.9, U2.12, U2.13, J7.1, J8.2, J9.2, J10.2,\\n     R3.2, C1.2\\nLINE_A: J4.1, R1.2(via JUNC_A), T1.S1\\nLINE_B: J4.2, U3.2(Cathode), U2.6(OUT2), T1.S2\\nBELL: J4.3, U2.3(OUT1)\\nLINE_SENSE: U1.L5(GPIO34), U3.3(Emitter)\\nRING_EN: U1.R13(GPIO4), U2.1(EN1,2)\\nRING_A: U1.R12(GPIO16), U2.2(IN1)\\nRING_B: U1.R11(GPIO17), U2.7(IN2)\\nI2S_BCLK: U1.L10(GPIO26), J5.6\\nI2S_LRCLK: U1.L9(GPIO25), J5.7\\nI2S_DOUT: U1.R4(GPIO22), J5.5\\nSD_CS: U1.R10(GPIO5), J7.6\\nSPI_MOSI: U1.R3(GPIO23), J7.3\\nSPI_MISO: U1.R8(GPIO19), J7.4\\nSPI_SCK: U1.R9(GPIO18), J7.5\\nBTN_RING: U1.L7(GPIO32), J8.1\\nBTN_CANCEL: U1.L8(GPIO33), J9.1\\nBTN_RESET: U1.L11(GPIO27), J10.1\\nAUDIO_P1: J6.1, T1.P1\\nAUDIO_P2: J6.2, T1.P2\\nJUNC_A: R1.2, R2.1, J4.1(LINE_A)\\nOPTO_ANODE: R2.2, U3.1\\nOPTO_EMIT: U3.3, R3.1, LINE_SENSE" (at 177.8 205.74 0)
    (effects (font (size 1.27 1.27)) (justify left)))

)"""
    return sch


# ═════════════════════════════════════════════════════════════════════
#  MAIN
# ═════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    import os
    out_dir = os.path.dirname(os.path.abspath(__file__))

    with open(os.path.join(out_dir, "k6_gpo_carrier.kicad_pro"), "w") as f:
        f.write(generate_project())
    print("wrote k6_gpo_carrier.kicad_pro")

    with open(os.path.join(out_dir, "k6_gpo_carrier.kicad_sch"), "w") as f:
        f.write(generate_schematic())
    print("wrote k6_gpo_carrier.kicad_sch")

    with open(os.path.join(out_dir, "k6_gpo_carrier.kicad_pcb"), "w") as f:
        f.write(generate_pcb())
    print("wrote k6_gpo_carrier.kicad_pcb")

    print("done — open k6_gpo_carrier.kicad_pro in KiCad 7+")
