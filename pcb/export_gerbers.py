#!/usr/bin/env python3
"""Export Gerber and drill files from the K6 carrier board PCB.

Run with: /usr/bin/python3 export_gerbers.py
"""

import pcbnew
import os

def main():
    pcb_path = "/home/ubuntu/repos/k6_gpo_exhibit/pcb/k6_carrier_rev2.kicad_pcb"
    gerber_dir = "/home/ubuntu/repos/k6_gpo_exhibit/pcb/gerbers"
    os.makedirs(gerber_dir, exist_ok=True)

    board = pcbnew.LoadBoard(pcb_path)

    # Fill zones before export
    filler = pcbnew.ZONE_FILLER(board)
    zones = board.Zones()
    filler.Fill(zones)

    # Gerber plot settings
    pctl = pcbnew.PLOT_CONTROLLER(board)
    popt = pctl.GetPlotOptions()

    popt.SetOutputDirectory(gerber_dir)
    popt.SetPlotFrameRef(False)
    popt.SetAutoScale(False)
    popt.SetScale(1)
    popt.SetMirror(False)
    popt.SetUseGerberAttributes(True)
    popt.SetUseGerberProtelExtensions(True)
    popt.SetExcludeEdgeLayer(True)
    popt.SetUseAuxOrigin(False)
    popt.SetSubtractMaskFromSilk(True)
    popt.SetDrillMarksType(pcbnew.PCB_PLOT_PARAMS.NO_DRILL_SHAPE)

    # Layer map: (layer_id, suffix, description)
    layers = [
        (pcbnew.F_Cu,    "F_Cu",    "Front copper"),
        (pcbnew.B_Cu,    "B_Cu",    "Back copper"),
        (pcbnew.F_SilkS, "F_SilkS", "Front silkscreen"),
        (pcbnew.B_SilkS, "B_SilkS", "Back silkscreen"),
        (pcbnew.F_Mask,  "F_Mask",  "Front solder mask"),
        (pcbnew.B_Mask,  "B_Mask",  "Back solder mask"),
        (pcbnew.Edge_Cuts, "Edge_Cuts", "Board outline"),
    ]

    for layer_id, suffix, desc in layers:
        pctl.OpenPlotfile(suffix, pcbnew.PLOT_FORMAT_GERBER, desc)
        pctl.SetLayer(layer_id)
        pctl.PlotLayer()
        print(f"  Exported: {suffix} ({desc})")

    pctl.ClosePlot()

    # Drill files
    drill = pcbnew.EXCELLON_WRITER(board)
    drill.SetOptions(False, False, board.GetDesignSettings().GetAuxOrigin(), False)
    drill.SetFormat(True)  # metric
    drill.CreateDrillandMapFilesSet(gerber_dir, True, False)
    print(f"  Exported: drill files")

    print(f"\nGerber files saved to: {gerber_dir}/")
    print("Ready for upload to JLCPCB, PCBWay, OSH Park, etc.")


if __name__ == "__main__":
    main()
