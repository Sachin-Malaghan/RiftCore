"""Writers: SVG and DXF drawings, the JSON model, schedules and quantities."""
import json
import math
import os

from . import model as M
from . import vastu as V

# layer: (stroke colour, stroke width in mm at 1:1 model scale, fill colour)
SVG_STYLE = {
    "WALL": ("#111111", 35, "#3a3a3a"), "CONCRETE": ("#111111", 35, "#8c8c8c"),
    "COLUMN": ("#000000", 25, "#000000"), "DOOR": ("#7a4a12", 20, None),
    "WINDOW": ("#1463b8", 20, None), "STAIR": ("#444444", 15, None),
    "DIM": ("#b03030", 12, None), "TAG": ("#1463b8", 10, None), "TEXT": ("#222222", 10, None),
    "ROOM": ("#000000", 10, None), "TITLE": ("#000000", 25, "#000000"), "GROUND": ("#5a4630", 45, None),
    "ROOF": ("#7a2f22", 30, None), "ROOFHATCH": ("#b98a80", 10, None), "SLAB": ("#666666", 15, None),
    "HIDDEN": ("#888888", 12, None), "GRID": ("#2f8f5f", 10, None), "FOOTING": ("#a05a00", 18, None),
    "BEAM": ("#777777", 10, None), "PLOT": ("#000000", 50, None), "SECTION": ("#b03030", 25, None),
    "LINE": ("#222222", 15, None),
}

DXF_COLOR = {"WALL": 7, "CONCRETE": 8, "COLUMN": 7, "DOOR": 30, "WINDOW": 5, "STAIR": 9, "DIM": 1,
             "TAG": 5, "TEXT": 7, "ROOM": 7, "TITLE": 7, "GROUND": 34, "ROOF": 12, "ROOFHATCH": 13,
             "SLAB": 8, "HIDDEN": 9, "GRID": 3, "FOOTING": 40, "BEAM": 8, "PLOT": 7, "SECTION": 1, "LINE": 7}


def bounds(prims):
    xs, ys = [], []
    for p in prims:
        k = p[0]
        if k == "line":
            xs += [p[1], p[3]]; ys += [p[2], p[4]]
        elif k == "rect":
            xs += [p[1], p[1] + p[3]]; ys += [p[2], p[2] + p[4]]
        elif k == "poly":
            xs += p[1][0::2]; ys += p[1][1::2]
        elif k == "arc":
            xs += [p[1] - p[3], p[1] + p[3]]; ys += [p[2] - p[3], p[2] + p[3]]
        elif k == "text":
            w = len(p[3]) * p[4] * 0.62
            if p[7]:                                   # vertical text
                xs += [p[1] - p[4] * 1.5, p[1] + p[4] * 0.5]; ys += [p[2] - w / 2.0, p[2] + w / 2.0]
            else:
                left = p[1] if p[6] == "l" else p[1] - w if p[6] == "r" else p[1] - w / 2.0
                xs += [left, left + w]; ys += [p[2] - p[4] * 0.4, p[2] + p[4] * 1.3]
    if not xs:
        return 0, 0, 1, 1
    return min(xs), min(ys), max(xs), max(ys)


def _esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def write_svg(sheet, path):
    x0, y0, x1, y1 = bounds(sheet.prims)
    pad = 600
    x0 -= pad; y0 -= pad; x1 += pad; y1 += pad
    w, h = x1 - x0, y1 - y0
    out = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="%.0f %.0f %.0f %.0f" '
           'width="%.0fmm" height="%.0fmm">' % (x0, -y1, w, h, w / 100.0, h / 100.0),
           '<rect x="%.0f" y="%.0f" width="%.0f" height="%.0f" fill="white"/>' % (x0, -y1, w, h)]
    for p in sheet.prims:
        k = p[0]
        layer = p[5] if k in ("line", "rect", "text") else p[2] if k == "poly" else p[6]
        stroke, sw, fill = SVG_STYLE.get(layer, SVG_STYLE["LINE"])
        st = 'stroke="%s" stroke-width="%d"' % (stroke, sw)
        if k == "line":
            out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" %s/>' % (p[1], -p[2], p[3], -p[4], st))
        elif k == "rect":
            f = (fill or "none") if p[6] else "none"
            out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s" %s/>'
                       % (p[1], -(p[2] + p[4]), p[3], p[4], f, st))
        elif k == "poly":
            pts = " ".join("%.1f,%.1f" % (p[1][i], -p[1][i + 1]) for i in range(0, len(p[1]), 2))
            f = (fill or stroke) if p[3] else "none"
            out.append('<polygon points="%s" fill="%s" %s/>' % (pts, f, st))
        elif k == "arc":
            cx, cy, r, a0, a1 = p[1], p[2], p[3], math.radians(p[4]), math.radians(p[5])
            sx, sy = cx + r * math.cos(a0), cy + r * math.sin(a0)
            ex, ey = cx + r * math.cos(a1), cy + r * math.sin(a1)
            out.append('<path d="M %.1f %.1f A %.1f %.1f 0 0 0 %.1f %.1f" fill="none" %s/>'
                       % (sx, -sy, r, r, ex, -ey, st))
        elif k == "text":
            anchor = {"l": "start", "c": "middle", "r": "end"}[p[6]]
            tr = ' transform="rotate(%d %.1f %.1f)"' % (-p[7], p[1], -p[2]) if p[7] else ""
            weight = ' font-weight="bold"' if layer in ("ROOM", "TITLE") else ""
            out.append('<text x="%.1f" y="%.1f" font-family="Arial, sans-serif" font-size="%.0f" '
                       'text-anchor="%s" fill="%s"%s%s>%s</text>'
                       % (p[1], -p[2], p[4] * 1.25, anchor, stroke, weight, tr, _esc(p[3])))
    out.append("</svg>")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(out))


def write_dxf(sheets, path, spacing=6000):
    """One DXF (R12 ASCII, millimetres) with the sheets laid out side by side."""
    e = []

    def ent(kind, layer, *pairs):
        e.extend(["0", kind, "8", layer])
        for code, value in pairs:
            e.extend([str(code), ("%.3f" % value) if isinstance(value, float) else str(value)])

    ox = 0.0
    used = set()
    for sheet in sheets:
        x0, y0, x1, y1 = bounds(sheet.prims)
        dx, dy = ox - x0, -y0
        for p in sheet.prims:
            k = p[0]
            if k == "line":
                used.add(p[5])
                ent("LINE", p[5], (10, p[1] + dx), (20, p[2] + dy), (11, p[3] + dx), (21, p[4] + dy))
            elif k == "rect":
                used.add(p[5])
                x, y, w, h = p[1] + dx, p[2] + dy, p[3], p[4]
                for a, b in (((x, y), (x + w, y)), ((x + w, y), (x + w, y + h)),
                             ((x + w, y + h), (x, y + h)), ((x, y + h), (x, y))):
                    ent("LINE", p[5], (10, float(a[0])), (20, float(a[1])), (11, float(b[0])), (21, float(b[1])))
                if p[6]:
                    ent("SOLID", p[5], (10, float(x)), (20, float(y)), (11, float(x + w)), (21, float(y)),
                        (12, float(x)), (22, float(y + h)), (13, float(x + w)), (23, float(y + h)))
            elif k == "poly":
                used.add(p[2])
                pts = [(p[1][i] + dx, p[1][i + 1] + dy) for i in range(0, len(p[1]), 2)]
                for a, b in zip(pts, pts[1:] + pts[:1]):
                    ent("LINE", p[2], (10, float(a[0])), (20, float(a[1])), (11, float(b[0])), (21, float(b[1])))
            elif k == "arc":
                used.add(p[6])
                ent("ARC", p[6], (10, float(p[1] + dx)), (20, float(p[2] + dy)), (40, float(p[3])),
                    (50, float(p[4])), (51, float(p[5])))
            elif k == "text":
                used.add(p[5])
                just = {"l": 0, "c": 1, "r": 2}[p[6]]
                x, y = float(p[1] + dx), float(p[2] + dy)
                ent("TEXT", p[5], (10, x), (20, y), (40, float(p[4])), (1, p[3]), (50, float(p[7])),
                    (72, just), (11, x), (21, y))
        ox += (x1 - x0) + spacing

    head = ["0", "SECTION", "2", "HEADER", "9", "$INSUNITS", "70", "4", "0", "ENDSEC",
            "0", "SECTION", "2", "TABLES", "0", "TABLE", "2", "LAYER", "70", str(len(used))]
    for layer in sorted(used):
        head += ["0", "LAYER", "2", layer, "70", "0", "62", str(DXF_COLOR.get(layer, 7)), "6", "CONTINUOUS"]
    head += ["0", "ENDTAB", "0", "ENDSEC", "0", "SECTION", "2", "ENTITIES"]
    with open(path, "w", encoding="ascii", errors="replace") as f:
        f.write("\n".join(head + e + ["0", "ENDSEC", "0", "EOF"]) + "\n")


# ----------------------------------------------------------------------
#  Quantities and report
# ----------------------------------------------------------------------

def quantities(d):
    H = d.H - M.SLAB_T
    brick = plaster = 0.0
    for w in d.walls:
        ops = [o for o in d.openings_on(w.floor, w.axis, w.pos) if o.start < w.b and o.end > w.a]
        void = sum(o.width * min(o.height, H) for o in ops)
        face = w.length * H - void
        brick += face * w.t / 1e9
        plaster += 2 * face / 1e6
    n = len(d.columns)
    col_h = d.floors * d.H + M.PLINTH + M.FOOTING_DEPTH - 300
    columns = n * M.COLUMN * M.COLUMN * col_h / 1e9
    brick -= n * M.COLUMN * M.COLUMN * d.floors * H / 1e9
    footings = n * d.footing * d.footing * 300 / 1e9
    beam_len = 4 * d.W + 2 * d.D
    plinth_beams = beam_len * 230 * 300 / 1e9
    slab_area = (d.W + M.EXT_T) * (d.D + M.EXT_T) / 1e6
    slabs = d.floors * slab_area * M.SLAB_T / 1000.0
    pcc = slab_area * 0.1
    floor_area = sum(r.area for r in d.rooms)
    steel = slabs * 80 + (columns + plinth_beams) * 150 + footings * 60
    doors = sum(1 for o in d.openings if o.kind in ("door", "garage"))
    windows = sum(1 for o in d.openings if o.kind == "window")
    return [
        ("Built-up area (all floors)", d.W * d.D * d.floors / 1e6, "sq.m"),
        ("Carpet area (clear room areas)", floor_area, "sq.m"),
        ("Brick masonry in walls", max(brick, 0.0), "cu.m"),
        ("RCC in floor / roof slabs", slabs, "cu.m"),
        ("RCC in columns", columns, "cu.m"),
        ("RCC in plinth beams", plinth_beams, "cu.m"),
        ("RCC in footings", footings, "cu.m"),
        ("PCC under ground floor", pcc, "cu.m"),
        ("Reinforcement steel (rule-of-thumb estimate)", steel, "kg"),
        ("Wall plaster, both faces", plaster, "sq.m"),
        ("Flooring", floor_area, "sq.m"),
        ("Doors", doors, "nos"),
        ("Windows", windows, "nos"),
    ]


def write_report(d, path):
    b = d.brief
    L = ["# %s" % b.name, ""]
    if b.prompt:
        L += ["> %s" % b.prompt, ""]
    L += ["| | |", "|---|---|",
          "| Storeys | %d |" % d.floors,
          "| Bedrooms / bathrooms | %d / %d |" % (b.bedrooms, b.bathrooms),
          "| Facing | %s%s |" % (b.facing.capitalize(), " (Vastu zoning)" if b.vastu else ""),
          "| House size (wall centre lines) | %d x %d mm |" % (d.W, d.D),
          "| Plot | %d x %d mm |" % (b.plot_w, b.plot_d),
          "| Floor to floor | %d mm (clear height %d) |" % (d.H, d.H - M.SLAB_T),
          "| Roof | %s |" % ("gable, %d deg" % M.ROOF_PITCH_DEG if b.style == "traditional"
                             else "flat RCC slab with %d parapet" % M.PARAPET),
          "| Top of building | %+.3f m |" % (d.top_level / 1000.0), ""]

    L += ["## Rooms", "", "| Floor | Room | Clear size (mm) | Area (sq.m) |" +
          (" Zone |" if b.vastu else ""), "|---|---|---|---|" + ("---|" if b.vastu else "")]
    for r in d.rooms:
        zone = " %s |" % V.zone_of(r.x + r.w / 2.0, r.y + r.d / 2.0, d.W, d.D, b.facing) if b.vastu else ""
        L.append("| %s | %s | %d x %d | %.2f |%s" % (d.floor_name(r.floor), r.name, r.clear_w, r.clear_d,
                                                    r.area, zone))
    L.append("")

    if b.vastu:
        L += ["## Vastu check (%s facing)" % b.facing, "", "| Guideline | Result | |", "|---|---|---|"]
        for rule, result, status in V.check(d):
            L.append("| %s | %s | %s |" % (rule, result, status))
        L += ["", "Vastu guidelines are traditional planning preferences, not building regulations.", ""]

    L += ["## Door and window schedule", "", "| Mark | Type | Width | Height | Sill | Nos |", "|---|---|---|---|---|---|"]
    sched = {}
    for o in d.openings:
        if o.kind == "opening":
            continue
        key = (o.tag, o.kind, o.width, o.height, o.sill)
        sched[key] = sched.get(key, 0) + 1
    for (tag, kind, w, h, sill), n in sorted(sched.items()):
        L.append("| %s | %s | %d | %d | %d | %d |" % (tag, kind, w, h, sill, n))
    L.append("")

    if d.stair:
        st = d.stair
        L += ["## Staircase", "", "Dog-leg, %d risers of %.1f mm, treads %d mm, flights %d mm wide, "
              "mid landing at %+.3f m above each floor." % (st.risers, st.riser_h, st.tread, st.flight_w,
                                                            d.H / 2000.0), ""]

    L += ["## Structure (preliminary)", "",
          "%d RCC columns %d x %d on isolated footings %d x %d x 300, %d mm below ground; plinth beams "
          "230 x 300 on all wall lines; %d mm RCC slabs; %d mm external and %d mm internal brick walls."
          % (len(d.columns), M.COLUMN, M.COLUMN, d.footing, d.footing, M.FOOTING_DEPTH, M.SLAB_T,
             M.EXT_T, M.INT_T), ""]

    L += ["## Preliminary quantities", "", "| Item | Quantity | Unit |", "|---|---|---|"]
    for item, qty, unit in quantities(d):
        L.append("| %s | %s | %s |" % (item, ("%d" % qty) if unit == "nos" else "%.2f" % qty, unit))
    L.append("")

    notes = list(b.notes) + list(d.notes)
    if notes:
        L += ["## Design notes", ""] + ["- %s" % n for n in notes] + [""]
    L += ["---", "Generated by RiftCore housegen. This is a preliminary scheme: sizes, structure and "
          "quantities must be checked by a licensed architect and structural engineer, and against the "
          "local building code, before construction."]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")


def design_to_dict(d, sheets):
    return {
        "brief": d.brief.to_dict(),
        "W": d.W, "D": d.D, "front_band": d.df, "hall": M.HALL,
        "floor_height": d.H, "plinth": M.PLINTH, "roof_level": d.roof_level, "top_level": d.top_level,
        "rooms": [dict(name=r.name, kind=r.kind, floor=r.floor, x=r.x, y=r.y, w=r.w, d=r.d,
                       clear_w=r.clear_w, clear_d=r.clear_d, area=round(r.area, 3)) for r in d.rooms],
        "walls": [dict(floor=w.floor, axis=w.axis, pos=w.pos, a=w.a, b=w.b, exterior=w.exterior,
                       thickness=w.t) for w in d.walls],
        "openings": [dict(kind=o.kind, floor=o.floor, axis=o.axis, pos=o.pos, start=o.start, width=o.width,
                          height=o.height, sill=o.sill, room=o.room, tag=o.tag) for o in d.openings],
        "columns": [list(c) for c in d.columns],
        "quantities": [list(q) for q in quantities(d)],
        "vastu": [list(r) for r in V.check(d)] if d.brief.vastu else [],
        "notes": list(d.brief.notes) + list(d.notes),
        "sheets": [dict(key=s.key, title=s.title, bounds=list(bounds(s.prims)), prims=s.prims)
                   for s in sheets],
    }


def write_all(d, sheets, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for s in sheets:
        write_svg(s, os.path.join(out_dir, s.key + ".svg"))
    write_dxf(sheets, os.path.join(out_dir, "drawings.dxf"))
    write_report(d, os.path.join(out_dir, "report.md"))
    with open(os.path.join(out_dir, "design.json"), "w", encoding="utf-8") as f:
        json.dump(design_to_dict(d, sheets), f)
    return out_dir
