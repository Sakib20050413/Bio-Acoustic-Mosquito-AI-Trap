#!/usr/bin/env python3
"""
Parametric 3D CAD Generator for Project 2 (CSE 326): WingBeat-TinyNet v5.0
Generates dimensionally accurate (millimeters) DIY 4-inch PVC + 2-inch Constricted Throat
+ Compact 70x90mm Perfboard Bay 3D meshes in BOTH .OBJ (named component groups for Blender)
and binary .STL (3D printing / CAD viewers). Pure NumPy — zero external deps!
"""
import os, struct
from pathlib import Path
import numpy as np

OUT_DIR = Path(r"d:\projects\Mosquito Project\enclosure_cad")
OUT_DIR.mkdir(parents=True, exist_ok=True)


def make_tube(r_outer, r_inner, z0, z1, segs=48, cx=0.0, cy=0.0, arc_deg=360.0):
    """Hollow cylinder / pipe along Z axis (dimensions in mm)."""
    ang = np.linspace(0.0, np.deg2rad(arc_deg), segs + 1)
    closed = (arc_deg >= 359.9)
    n_ring = segs if closed else segs + 1
    tris = []
    for i in range(segs):
        a0, a1 = ang[i], ang[i + 1]
        c0, s0, c1, s1 = np.cos(a0), np.sin(a0), np.cos(a1), np.sin(a1)
        # Outer wall
        p00 = (cx + r_outer * c0, cy + r_outer * s0, z0)
        p10 = (cx + r_outer * c1, cy + r_outer * s1, z0)
        p11 = (cx + r_outer * c1, cy + r_outer * s1, z1)
        p01 = (cx + r_outer * c0, cy + r_outer * s0, z1)
        tris += [(p00, p10, p11), (p00, p11, p01)]
        # Inner wall (reversed winding)
        q00 = (cx + r_inner * c0, cy + r_inner * s0, z0)
        q10 = (cx + r_inner * c1, cy + r_inner * s1, z0)
        q11 = (cx + r_inner * c1, cy + r_inner * s1, z1)
        q01 = (cx + r_inner * c0, cy + r_inner * s0, z1)
        tris += [(q00, q11, q10), (q00, q01, q11)]
        # Top & bottom rims
        tris += [(p01, p11, q11), (p01, q11, q01)]
        tris += [(p00, q10, p10), (p00, q00, q10)]
    return tris


def make_funnel(r_top_out, r_bot_out, wall, z_bot, z_top, segs=48):
    """Conical frustum funnel reducer (4-inch 100mm -> 2-inch 50mm PVC reducer)."""
    ang = np.linspace(0.0, 2.0 * np.pi, segs + 1)
    r_top_in = max(2.0, r_top_out - wall)
    r_bot_in = max(2.0, r_bot_out - wall)
    tris = []
    for i in range(segs):
        a0, a1 = ang[i], ang[i + 1]
        c0, s0, c1, s1 = np.cos(a0), np.sin(a0), np.cos(a1), np.sin(a1)
        b0 = (r_bot_out * c0, r_bot_out * s0, z_bot)
        b1 = (r_bot_out * c1, r_bot_out * s1, z_bot)
        t1 = (r_top_out * c1, r_top_out * s1, z_top)
        t0 = (r_top_out * c0, r_top_out * s0, z_top)
        bi0 = (r_bot_in * c0, r_bot_in * s0, z_bot)
        bi1 = (r_bot_in * c1, r_bot_in * s1, z_bot)
        ti1 = (r_top_in * c1, r_top_in * s1, z_top)
        ti0 = (r_top_in * c0, r_top_in * s0, z_top)
        tris += [(b0, b1, t1), (b0, t1, t0)]
        tris += [(bi0, ti1, bi1), (bi0, ti0, ti1)]
        tris += [(t0, t1, ti1), (t0, ti1, ti0)]
        tris += [(b0, bi1, b1), (b0, bi0, bi1)]
    return tris


def make_box(x0, x1, y0, y1, z0, z1):
    """Axis-aligned solid rectangular prism (mm)."""
    v = [
        (x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
        (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1),
    ]
    faces = [
        (0, 2, 1), (0, 3, 2),  # bottom
        (4, 5, 6), (4, 6, 7),  # top
        (0, 1, 5), (0, 5, 4),  # front
        (2, 3, 7), (2, 7, 6),  # back
        (0, 4, 7), (0, 7, 3),  # left
        (1, 2, 6), (1, 6, 5),  # right
    ]
    return [(v[a], v[b], v[c]) for a, b, c in faces]


def make_cyl_solid(r, z0, z1, segs=24, cx=0.0, cy=0.0):
    ang = np.linspace(0.0, 2.0 * np.pi, segs + 1)
    tris = []
    cb, ct = (cx, cy, z0), (cx, cy, z1)
    for i in range(segs):
        a0, a1 = ang[i], ang[i + 1]
        p0 = (cx + r * np.cos(a0), cy + r * np.sin(a0), z0)
        p1 = (cx + r * np.cos(a1), cy + r * np.sin(a1), z0)
        q0 = (cx + r * np.cos(a0), cy + r * np.sin(a0), z1)
        q1 = (cx + r * np.cos(a1), cy + r * np.sin(a1), z1)
        tris += [(p0, p1, q1), (p0, q1, q0), (ct, q0, q1), (cb, p1, p0)]
    return tris


# === PARAMETRIC DIMENSIONS (ALL IN MILLIMETERS — REAL BANGLADESH HARDWARE STORE PVC SIZES) ===
# Z = 0 mm is bottom base of Removable Mesh Capture Cup; total trap height = 240 mm!
groups = {}

# 1. Outer 4-inch (110mm OD / 104mm ID) PVC Outer Housing Shell (Z = 75..235 mm)
groups["Outer_4in_PVC_Housing_110mm"] = make_tube(55.0, 52.0, 75.0, 235.0, segs=64)

# 2. Removable Bottom Capture Cup + Fine Mesh Screen Base (4-inch PVC End Cap / Coupler, Z = 0..75 mm)
cup = make_tube(55.0, 52.0, 4.0, 75.0, segs=64)
cup += make_tube(55.0, 20.0, 0.0, 4.0, segs=48)  # Bottom ventilated mesh ring
groups["Removable_Mesh_Capture_Cup"] = cup

# 3. Top Conical Intake Funnel (4" -> 2" PVC Reducer, 104mm -> 50mm ID, Z = 185..235 mm)
groups["Top_Funnel_Reducer_100to50mm"] = make_funnel(52.0, 25.0, 2.8, 185.0, 235.0, segs=64)

# 4. Constricted 2-inch (50mm OD / 45mm ID) Optical & Acoustic Listening Throat Tube (Z = 105..185 mm)
#    45mm internal diameter eliminates HW-201 IR Gate blind spot!
groups["Inner_2in_PVC_Throat_45mm_ID"] = make_tube(25.0, 22.5, 105.0, 185.0, segs=48)

# 5. Lower Fan Shroud / Exhaust Duct (Z = 65..105 mm)
groups["Fan_Shroud_Duct_45mm"] = make_tube(25.0, 22.5, 65.0, 105.0, segs=48)

# 6. 6x 5mm 395nm UV LED Ring mounted evenly around top intake rim (radius = 44mm, Z = 225..236 mm)
uv_leds = []
for k in range(6):
    th = np.deg2rad(k * 60.0 + 30.0)
    uv_leds += make_cyl_solid(2.5, 226.0, 237.0, segs=16, cx=44.0 * np.cos(th), cy=44.0 * np.sin(th))
groups["UV_395nm_LED_Ring_6x"] = uv_leds

# 7. 100 Ohm 5W Ceramic Thermal Cue Resistor (22 x 10 x 10 mm) at intake rim (Z = 216..226 mm)
groups["Thermal_100Ohm_5W_Resistor"] = make_box(-11.0, 11.0, 34.0, 44.0, 216.0, 226.0)

# 8. INMP441 I2S Mic + 4mm Acoustic Foam Isolation Gasket mounted on Throat Left Wall (-X, Z = 148mm)
groups["INMP441_Mic_and_Foam_Gasket"] = make_box(-33.0, -23.0, -7.5, 7.5, 140.0, 155.0)

# 9. DHT22 Ambient Temp/Humidity Sensor mounted in Upper Throat Vent (+Y, Z = 165mm)
groups["DHT22_Temp_Humidity_Sensor"] = make_box(-7.5, 7.5, 24.0, 32.0, 158.0, 178.0)

# 10. HW-201 IR Gate #1 (Entry dt_ms Pollinator Veto) at Throat Neck (+X, Z = 132mm)
groups["HW201_IR_Gate1_Entry_BeeVeto"] = make_box(23.0, 44.0, -7.0, 7.0, 126.0, 138.0)

# 11. 5V Brushless Suction Fan (JD-4007S5L2, 40 x 40 x 10 mm) inside Throat Base (Z = 92..104 mm)
fan = make_box(-20.0, 20.0, -20.0, 20.0, 92.0, 102.0)
fan += make_cyl_solid(7.0, 91.0, 103.0, segs=20)
groups["Suction_Fan_5V_40mm"] = fan

# 12. HW-201 IR Gate #2 (Mesh Catch Verification) directly below Fan / above Mesh Cup (+X, Z = 68mm)
groups["HW201_IR_Gate2_Mesh_Catch"] = make_box(23.0, 44.0, -7.0, 7.0, 62.0, 74.0)

# 13. Side Compact Electronics Bay Housing (holds 70x90mm Perfboard + ESP32-S3 + 5V Relay, mounted at -Y side)
bay = make_box(-40.0, 40.0, -90.0, -54.0, 86.0, 186.0)
groups["Side_Electronics_Bay_Compact_PCB"] = bay

# 14. Front Face UI Panel: 1.3" SH1106 OLED (35x35mm) + 3x 10mm Status LEDs + 4x Yellow Buttons
ui = make_box(-18.0, 18.0, -93.0, -90.0, 146.0, 176.0)  # OLED Bezel
for idx_l, lx in enumerate([-18.0, 0.0, 18.0]):
    ui += make_box(lx - 4.0, lx + 4.0, -95.0, -90.0, 124.0, 132.0)  # 3x Status LEDs
for idx_b, bx in enumerate([-24.0, -8.0, 8.0, 24.0]):
    ui += make_box(bx - 4.5, bx + 4.5, -94.0, -90.0, 102.0, 111.0)  # 4x Yellow Tactile Buttons
groups["Front_OLED_3LEDs_4Buttons_UI"] = ui

# Write Wavefront .OBJ (with named object groups for Blender / FreeCAD / MeshLab)
obj_path = OUT_DIR / "WingBeat_v5_PVC_Trap_Assembly.obj"
with open(obj_path, "w", encoding="utf-8") as f:
    f.write("# WingBeat-TinyNet v5.0 — DIY 4-inch PVC + 2-inch Constricted Throat Trap CAD (Units: mm)\n")
    f.write("# BAIUST CSE 326 Team: Sakib (1119003), Ifag (1119004), Towhid (1119007)\n\n")
    v_idx = 1
    for gname, tris in groups.items():
        f.write(f"o {gname}\n")
        for p0, p1, p2 in tris:
            f.write(f"v {p0[0]:.3f} {p0[1]:.3f} {p0[2]:.3f}\n")
            f.write(f"v {p1[0]:.3f} {p1[1]:.3f} {p1[2]:.3f}\n")
            f.write(f"v {p2[0]:.3f} {p2[1]:.3f} {p2[2]:.3f}\n")
            f.write(f"f {v_idx} {v_idx+1} {v_idx+2}\n")
            v_idx += 3

# Write Binary .STL (universally opens in 3D Printers / Cura / Windows 3D Viewer / SolidWorks)
all_tris = [t for tris in groups.values() for t in tris]
stl_path = OUT_DIR / "WingBeat_v5_PVC_Trap_Assembly.stl"
with open(stl_path, "wb") as f:
    hdr = b"WingBeat-TinyNet v5.0 DIY PVC Mosquito Trap CAD (mm) - BAIUST CSE 326"
    f.write(hdr.ljust(80, b"\x00")[:80])
    f.write(struct.pack("<I", len(all_tris)))
    for p0, p1, p2 in all_tris:
        u = np.subtract(p1, p0)
        w = np.subtract(p2, p0)
        n = np.cross(u, w)
        norm = float(np.linalg.norm(n))
        n = n / norm if norm > 1e-9 else np.array([0.0, 0.0, 1.0])
        f.write(struct.pack("<12fH", n[0], n[1], n[2], p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], p2[0], p2[1], p2[2], 0))

print(f"[OK] Generated 3D CAD Assembly ({len(groups)} named components, {len(all_tris)} triangles):")
print(f"     OBJ (Blender-ready): {obj_path} ({obj_path.stat().st_size:,} B)")
print(f"     STL (3D Print/CAD) : {stl_path} ({stl_path.stat().st_size:,} B)")
