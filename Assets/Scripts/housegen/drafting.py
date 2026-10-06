"""2D drawings generated from the Design.

Every sheet is a flat list of primitives in millimetres (Y up), so the SVG
and DXF writers and the editor's drawing viewer all show exactly the same
linework:

    ["line", x1, y1, x2, y2, layer]
    ["rect", x, y, w, h, layer, filled]
    ["poly", [x, y, x, y, ...], layer, filled]
    ["arc",  cx, cy, r, a0_deg, a1_deg, layer]
    ["text", x, y, string, height, layer, anchor("l"|"c"|"r"), rotation_deg]
"""
from . import model as M
from . import vastu as V


class Sheet:
    def __init__(self, key, title):
        self.key = key
        self.title = title
        self.prims = []

    def line(self, x1, y1, x2, y2, layer="LINE"):
        self.prims.append(["line", x1, y1, x2, y2, layer])

    def rect(self, x, y, w, h, layer="LINE", fill=False):
        self.prims.append(["rect", x, y, w, h, layer, bool(fill)])

    def poly(self, pts, layer="LINE", fill=False):
        flat = [c for p in pts for c in p]
        self.prims.append(["poly", flat, layer, bool(fill)])

    def arc(self, cx, cy, r, a0, a1, layer="DOOR"):
        self.prims.append(["arc", cx, cy, r, a0, a1, layer])

    def text(self, x, y, s, h=200, layer="TEXT", anchor="c", rot=0):
        self.prims.append(["text", x, y, str(s), h, layer, anchor, rot])

    def dashed(self, x1, y1, x2, y2, layer="HIDDEN", dash=300, gap=180):
        length = ((x2 - x1) ** 2 + (y2 - y1) ** 2) ** 0.5
        if length <= 0:
            return
        ux, uy = (x2 - x1) / length, (y2 - y1) / length
        t = 0.0
        while t < length:
            e = min(t + dash, length)
            self.line(x1 + ux * t, y1 + uy * t, x1 + ux * e, y1 + uy * e, layer)
            t += dash + gap

    # Dimension lines carry the exact model distance in millimetres.
    def dim_h(self, x1, x2, y, label=None):
        if abs(x2 - x1) < 1:
            return
        self.line(x1, y, x2, y, "DIM")
        for x in (x1, x2):
            self.line(x, y - 130, x, y + 130, "DIM")
            self.line(x - 90, y - 90, x + 90, y + 90, "DIM")
        self.text((x1 + x2) / 2.0, y + 70, label or "%d" % round(abs(x2 - x1)), 170, "DIM", "c")

    def dim_v(self, y1, y2, x, label=None):
        if abs(y2 - y1) < 1:
            return
        self.line(x, y1, x, y2, "DIM")
        for y in (y1, y2):
            self.line(x - 130, y, x + 130, y, "DIM")
            self.line(x - 90, y - 90, x + 90, y + 90, "DIM")
        self.text(x - 70, (y1 + y2) / 2.0, label or "%d" % round(abs(y2 - y1)), 170, "DIM", "c", 90)

    def chain_h(self, xs, y):
        xs = sorted(set(xs))
        for a, b in zip(xs, xs[1:]):
            self.dim_h(a, b, y)

    def chain_v(self, ys, x):
        ys = sorted(set(ys))
        for a, b in zip(ys, ys[1:]):
            self.dim_v(a, b, x)

    def title_block(self, x, y, design, scale="1:100"):
        self.text(x, y, self.title.upper(), 420, "TITLE", "l")
        self.text(x, y - 420, "%s   |   scale %s   |   all dimensions in millimetres"
                  % (design.brief.name, scale), 200, "TEXT", "l")

    def level(self, x, y, label):
        self.line(x, y, x + 900, y, "DIM")
        self.poly([(x + 250, y), (x + 400, y + 220), (x + 550, y)], "DIM")
        self.text(x + 1000, y - 80, label, 180, "DIM", "l")


def _fmt_level(mm):
    return "%+.3f" % (mm / 1000.0)


def _solid_spans(a, b, openings):
    """Parts of a wall [a, b] left solid after its openings are cut out."""
    spans, cur = [], a
    for o in sorted(openings, key=lambda o: o.start):
        if o.start > cur:
            spans.append((cur, min(o.start, b)))
        cur = max(cur, o.end)
    if cur < b:
        spans.append((cur, b))
    return spans


# ----------------------------------------------------------------------
#  Floor plans
# ----------------------------------------------------------------------

def floor_plan(d, floor):
    s = Sheet("plan_%d" % floor, "%s Plan" % d.floor_name(floor))
    W, D = d.W, d.D

    # Walls, cut at openings.
    for w in (w for w in d.walls if w.floor == floor):
        ops = [o for o in d.openings_on(floor, w.axis, w.pos) if o.start < w.b and o.end > w.a]
        ext = M.EXT_T / 2.0 if w.exterior else 0.0
        spans = _solid_spans(w.a, w.b, ops)
        for i, (a, b) in enumerate(spans):
            a2 = a - ext if a == w.a else a
            b2 = b + ext if b == w.b else b
            if w.axis == "h":
                s.rect(a2, w.pos - w.t / 2.0, b2 - a2, w.t, "WALL", True)
            else:
                s.rect(w.pos - w.t / 2.0, a2, w.t, b2 - a2, "WALL", True)

    # Openings.
    for o in d.openings_on(floor):
        t = M.EXT_T if o.pos in (0, W if o.axis == "v" else D) else M.INT_T
        mid = o.start + o.width / 2.0
        if o.axis == "h":
            inward = 1 if o.pos == 0 else -1 if o.pos == D else -o.swing
            if o.kind == "window":
                s.rect(o.start, o.pos - t / 2.0, o.width, t, "WINDOW")
                s.line(o.start, o.pos, o.end, o.pos, "WINDOW")
                s.text(mid, o.pos + inward * 420 - 80, o.tag, 160, "TAG")
            elif o.kind == "door":
                s.line(o.start, o.pos, o.start, o.pos + o.swing * o.width, "DOOR")
                if o.swing > 0:
                    s.arc(o.start, o.pos, o.width, 0, 90, "DOOR")
                else:
                    s.arc(o.start, o.pos, o.width, 270, 360, "DOOR")
                s.text(mid, o.pos - o.swing * 330 - 80, o.tag, 160, "TAG")
            elif o.kind == "garage":
                s.dashed(o.start, o.pos, o.end, o.pos, "DOOR")
                s.text(mid, o.pos + 300, o.tag, 160, "TAG")
        else:
            inward = 1 if o.pos == 0 else -1
            if o.kind == "window":
                s.rect(o.pos - t / 2.0, o.start, t, o.width, "WINDOW")
                s.line(o.pos, o.start, o.pos, o.end, "WINDOW")
                s.text(o.pos + inward * 480, mid - 80, o.tag, 160, "TAG")

    # Columns.
    c = M.COLUMN / 2.0
    for x, y in d.columns:
        s.rect(x - c, y - c, M.COLUMN, M.COLUMN, "COLUMN", True)

    # Staircase (dog-leg: up the right flight, mid landing, back on the left).
    st = d.stair
    if st:
        top = floor == d.floors - 1
        per = st.risers // 2
        k = st.dir
        xl, xr = st.x + 100, st.x + 100 + st.flight_w + 100
        for i in range(per):
            y = st.y0 + k * i * st.tread
            s.line(xr, y, xr + st.flight_w, y, "STAIR")
            s.line(xl, y, xl + st.flight_w, y, "STAIR")
        far = st.y + st.d - 60 if k > 0 else st.y + 60
        land = min(1100, abs(far - st.landing_y))
        s.rect(xl, min(st.landing_y, st.landing_y + k * land), st.flight_w * 2 + 100, land, "STAIR")
        s.line(xl + st.flight_w + 50, st.y0, xl + st.flight_w + 50, st.landing_y, "STAIR")
        ax = xr + st.flight_w / 2.0
        tip = st.landing_y - k * 150
        s.line(ax, st.y0 + k * 100, ax, tip, "STAIR")
        s.poly([(ax - 110, tip - k * 230), (ax, tip), (ax + 110, tip - k * 230)], "STAIR")
        s.text(ax, st.y0 - k * 230 - 80, "DN" if top else "UP", 170, "TEXT")
        s.text(st.x + st.w / 2.0, (st.y + st.d - 420) if k > 0 else (st.y + 250),
               "%d R x %.0f / T %d" % (st.risers, st.riser_h, st.tread), 130, "TEXT")

    # Room labels: name, clear size, clear area.
    for r in d.rooms_on(floor):
        cx, cy = r.x + r.w / 2.0, r.y + r.d / 2.0
        if r.kind == "stair":
            continue
        if r.kind == "hall":
            s.text(cx, cy - 70, "%s  %d wide" % (r.name.upper(), r.clear_d), 170, "TEXT")
            continue
        small = r.w < 2100
        s.text(cx, cy + (150 if small else 260), r.name.upper(), 170 if small else 230, "ROOM")
        s.text(cx, cy - 110, "%d x %d" % (r.clear_w, r.clear_d), 140 if small else 170, "TEXT")
        s.text(cx, cy - 400, "%.2f sq.m" % r.area, 140 if small else 170, "TEXT")

    # Dimension chains (wall centre lines) and overall sizes.
    rooms = d.rooms_on(floor)
    fx = [0, W] + [r.x for r in rooms if r.y == 0 and r.kind != "hall"]
    bx = [0, W] + [r.x for r in rooms if r.y == d.y_back]
    s.chain_h(fx, -1000)
    s.dim_h(0, W, -1700)
    s.chain_h(bx, D + 1000)
    s.dim_h(0, W, D + 1700)
    s.chain_v([0, d.y_hall, d.y_back, D], -1000)
    s.dim_v(0, D, -1700)

    # Section mark.
    cx = _section_x(d)
    s.dashed(cx, -600, cx, D + 600, "SECTION", 500, 200)
    s.text(cx, -900, "A", 260, "SECTION")
    s.text(cx, D + 640, "A", 260, "SECTION")
    if floor == 0:
        s.text(W / 2.0, -2500, "ROAD / FRONT  (%s)" % d.brief.facing.upper(), 220, "TEXT")
    _north_arrow(s, W + 2600, D - 900, d.brief.facing)

    s.title_block(0, -3200, d)
    return s


def _north_arrow(s, cx, cy, facing):
    nx, ny = V.north_vector(facing)
    L = 800
    tx, ty = cx + nx * L, cy + ny * L
    s.line(cx - nx * L, cy - ny * L, tx, ty, "TITLE")
    px, py = -ny, nx                                   # perpendicular
    s.poly([(tx, ty), (tx - nx * 380 + px * 200, ty - ny * 380 + py * 200),
            (tx - nx * 380 - px * 200, ty - ny * 380 - py * 200)], "TITLE", True)
    s.text(cx + nx * (L + 330), cy + ny * (L + 330) - 130, "N", 300, "TITLE")


def _section_x(d):
    """Section A-A is cut through the living room, clear of the stair."""
    living = next((r for r in d.rooms_on(0) if r.kind == "living"), None)
    return snap_half(living.x + living.w / 2.0) if living else d.W / 2.0


def snap_half(v):
    return int(round(v / 50.0)) * 50


# ----------------------------------------------------------------------
#  Elevations
# ----------------------------------------------------------------------

FACES = {
    "front": ("h", "zero", "Front Elevation"),
    "rear":  ("h", "far",  "Rear Elevation"),
    "left":  ("v", "zero", "Left Side Elevation"),
    "right": ("v", "far",  "Right Side Elevation"),
}


def elevation(d, face):
    axis, side, title = FACES[face]
    title = "%s (%s)" % (title, V.face_compass(d.brief.facing)[face].capitalize())
    s = Sheet("elev_" + face, title)
    L = d.W if axis == "h" else d.D
    pos = 0 if side == "zero" else (d.D if axis == "h" else d.W)
    # Viewed from outside, the rear and left faces read mirrored.
    mirror = face in ("rear", "left")

    def hx(v):
        return L - v if mirror else v

    e = M.EXT_T / 2.0
    s.line(-2500, 0, L + 2500, 0, "GROUND")
    s.rect(-e, 0, L + 2 * e, M.PLINTH, "WALL")
    for f in range(d.floors):
        z = d.ffl(f)
        s.rect(-e, z, L + 2 * e, d.H, "WALL")
        s.line(-e, z + d.H - M.SLAB_T, L + e, z + d.H - M.SLAB_T, "SLAB")
        for o in d.openings_on(f, axis, pos):
            a, b = sorted((hx(o.start), hx(o.end)))
            zb = z + o.sill
            if o.kind == "window":
                s.rect(a, zb, b - a, o.height, "WINDOW")
                s.line((a + b) / 2.0, zb, (a + b) / 2.0, zb + o.height, "WINDOW")
                s.line(a, zb + o.height * 0.5, b, zb + o.height * 0.5, "WINDOW")
                s.rect(a - 60, zb - 60, b - a + 120, 60, "WINDOW")
            elif o.kind == "door":
                s.rect(a, zb, b - a, o.height, "DOOR")
                s.rect(a + 120, zb + 150, b - a - 240, o.height - 300, "DOOR")
            elif o.kind == "garage":
                s.rect(a, zb, b - a, o.height, "DOOR")
                for k in range(1, 6):
                    s.line(a, zb + o.height * k / 6.0, b, zb + o.height * k / 6.0, "DOOR")
            s.text((a + b) / 2.0, zb + o.height + 60, o.tag, 130, "TAG")

    roof = d.roof_level
    oh = M.ROOF_OVERHANG
    if d.brief.style == "traditional":
        ridge = roof + d.ridge_height
        if axis == "h":       # ridge runs across: the roof slope faces the viewer
            s.rect(-oh, roof, L + 2 * oh, d.ridge_height, "ROOF")
            for k in range(1, 6):
                s.line(-oh, roof + d.ridge_height * k / 6.0, L + oh, roof + d.ridge_height * k / 6.0, "ROOFHATCH")
        else:
            s.poly([(-oh, roof), (L / 2.0, ridge), (L + oh, roof)], "ROOF")
            s.line(-oh, roof, L + oh, roof, "ROOF")
    else:
        s.rect(-e, roof, L + 2 * e, M.PARAPET, "ROOF")
        s.line(-e, roof + M.PARAPET - 80, L + e, roof + M.PARAPET - 80, "ROOF")

    # Levels and overall dimensions.
    lx = L + 1400
    s.level(lx, 0, "%s  GL" % _fmt_level(0))
    for f in range(d.floors):
        s.level(lx, d.ffl(f), "%s  %s FFL" % (_fmt_level(d.ffl(f)), d.floor_name(f)))
    s.level(lx, roof, "%s  Roof slab" % _fmt_level(roof))
    s.level(lx, d.top_level, "%s  %s" % (_fmt_level(d.top_level),
            "Ridge" if d.brief.style == "traditional" else "Parapet top"))
    s.dim_h(0, L, -900)
    s.chain_v([0] + [d.ffl(f) for f in range(d.floors)] + [roof, d.top_level], -1300)
    s.title_block(0, -2100, d)
    return s


# ----------------------------------------------------------------------
#  Section A-A (cut front to rear)
# ----------------------------------------------------------------------

def section(d):
    s = Sheet("section", "Section A-A")
    D = d.D
    cx = _section_x(d)
    lines = [(0, M.EXT_T), (d.y_hall, M.INT_T), (d.y_back, M.INT_T), (D, M.EXT_T)]

    s.line(-2500, 0, D + 2500, 0, "GROUND")
    # Foundations: footing, column stub, plinth beam under each wall line.
    for y, t in lines:
        f = d.footing
        s.rect(y - f / 2.0, -M.FOOTING_DEPTH, f, 300, "CONCRETE", True)
        s.rect(y - M.COLUMN / 2.0, -M.FOOTING_DEPTH + 300, M.COLUMN, M.FOOTING_DEPTH - 300 + M.PLINTH - 300, "CONCRETE", True)
        s.rect(y - M.COLUMN / 2.0, M.PLINTH - 300, M.COLUMN, 300, "CONCRETE", True)
    s.rect(0, M.PLINTH - 100, D, 100, "CONCRETE", True)
    s.text(D / 2.0, M.PLINTH / 2.0 - 160, "COMPACTED FILL + 100 PCC", 130, "TEXT")

    for f in range(d.floors):
        z = d.ffl(f)
        top = z + d.H - M.SLAB_T
        s.rect(-M.EXT_T / 2.0, top, D + M.EXT_T, M.SLAB_T, "CONCRETE", True)
        for y, t in lines:
            cut = [o for o in d.openings_on(f, "h", y) if o.start <= cx <= o.end]
            if cut:                       # the cut passes through an opening
                o = cut[0]
                if o.sill > 0:
                    s.rect(y - t / 2.0, z, t, o.sill, "WALL", True)
                head = z + o.sill + o.height
                s.rect(y - t / 2.0, head, t, top - head, "WALL", True)
                s.rect(y - t / 2.0, z + o.sill, t, o.height, "WINDOW" if o.kind == "window" else "DOOR")
            else:
                s.rect(y - t / 2.0, z, t, top - z, "WALL", True)
        s.dashed(0, z + M.LINTEL, D, z + M.LINTEL, "HIDDEN")
        for r in d.rooms_on(f):
            if r.x <= cx <= r.x2:
                s.text(r.y + r.d / 2.0, z + d.H * 0.42, r.name.upper(), 170 if r.kind != "hall" else 120, "ROOM")
                s.text(r.y + r.d / 2.0, z + d.H * 0.42 - 300, "clear ht %d" % (d.H - M.SLAB_T), 120, "TEXT")

    roof = d.roof_level
    if d.brief.style == "traditional":
        oh = M.ROOF_OVERHANG
        s.poly([(-oh, roof), (D / 2.0, roof + d.ridge_height), (D + oh, roof)], "ROOF")
        s.poly([(-oh + 200, roof), (D / 2.0, roof + d.ridge_height - 110), (D + oh - 200, roof)], "ROOF")
        s.text(D / 2.0, roof + d.ridge_height * 0.3, "ROOF TRUSS / TILES  %d deg" % M.ROOF_PITCH_DEG, 130, "TEXT")
    else:
        s.rect(-M.EXT_T / 2.0, roof, M.EXT_T, M.PARAPET, "WALL", True)
        s.rect(D - M.EXT_T / 2.0, roof, M.EXT_T, M.PARAPET, "WALL", True)

    lx = D + 1500
    s.level(lx, -M.FOOTING_DEPTH, "%s  Footing base" % _fmt_level(-M.FOOTING_DEPTH))
    s.level(lx, 0, "%s  GL" % _fmt_level(0))
    for f in range(d.floors):
        s.level(lx, d.ffl(f), "%s  %s FFL" % (_fmt_level(d.ffl(f)), d.floor_name(f)))
    s.level(lx, roof, "%s  Roof slab" % _fmt_level(roof))
    s.level(lx, d.top_level, "%s  Top" % _fmt_level(d.top_level))
    s.chain_h([0, d.y_hall, d.y_back, D], -M.FOOTING_DEPTH - 700)
    s.dim_h(0, D, -M.FOOTING_DEPTH - 1300)
    s.chain_v([-M.FOOTING_DEPTH, 0] + [d.ffl(f) for f in range(d.floors)] + [roof, d.top_level], -1500)
    s.title_block(0, -M.FOOTING_DEPTH - 2200, d)
    return s


# ----------------------------------------------------------------------
#  Structure and site
# ----------------------------------------------------------------------

def structure_plan(d):
    s = Sheet("structure", "Foundation and Column Layout")
    W, D = d.W, d.D
    xs = sorted({x for x, _ in d.columns})
    ys = sorted({y for _, y in d.columns})
    for i, x in enumerate(xs):
        s.dashed(x, -800, x, D + 800, "GRID", 400, 150)
        s.text(x, D + 950, str(i + 1), 240, "GRID")
    for j, y in enumerate(ys):
        s.dashed(-800, y, W + 800, y, "GRID", 400, 150)
        s.text(-1050, y - 110, chr(ord("A") + j), 240, "GRID")
    f, c = d.footing, M.COLUMN / 2.0
    for n, (x, y) in enumerate(d.columns):
        s.rect(x - f / 2.0, y - f / 2.0, f, f, "FOOTING")
        s.rect(x - c, y - c, M.COLUMN, M.COLUMN, "COLUMN", True)
        s.text(x + 200, y + 200, "C%d" % (n + 1), 150, "TAG", "l")
    for y in ys:                                   # plinth beams along wall lines
        s.line(0, y - 115, W, y - 115, "BEAM")
        s.line(0, y + 115, W, y + 115, "BEAM")
    for x in (0, W):
        s.line(x - 115, 0, x - 115, D, "BEAM")
        s.line(x + 115, 0, x + 115, D, "BEAM")
    s.chain_h(xs, -1500)
    s.dim_h(0, W, -2200)
    s.chain_v(ys, -1700)
    s.dim_v(0, D, -2400)
    s.text(0, -3000, "Columns %d x %d RCC. Isolated footings %d x %d x 300 at %d below GL. "
           "Plinth beams 230 x 300 on all wall lines." % (M.COLUMN, M.COLUMN, f, f, M.FOOTING_DEPTH), 170, "TEXT", "l")
    s.text(0, -3350, "Sizes are indicative for a preliminary scheme - to be confirmed by a structural engineer.",
           170, "TEXT", "l")
    s.title_block(0, -4100, d)
    return s


def site_plan(d):
    b = d.brief
    s = Sheet("site", "Site Plan")
    side = max(b.setback_side, (b.plot_w - d.W) / 2.0)
    px, py = -side, -b.setback_front
    s.rect(px, py, b.plot_w, b.plot_d, "PLOT")
    s.rect(px + b.setback_side, py + b.setback_front, b.plot_w - 2 * b.setback_side,
           b.plot_d - b.setback_front - b.setback_rear, "HIDDEN")
    s.rect(0, 0, d.W, d.D, "WALL")
    s.line(0, 0, d.W, d.D, "ROOFHATCH")
    s.line(0, d.D, d.W, 0, "ROOFHATCH")
    s.text(d.W / 2.0, d.D / 2.0 + 250, "PROPOSED HOUSE", 300, "ROOM")
    s.text(d.W / 2.0, d.D / 2.0 - 250, "%d x %d  (G%s)" % (d.W, d.D, "+%d" % (d.floors - 1) if d.floors > 1 else ""),
           220, "TEXT")
    s.rect(px - 1000, py - 4500, b.plot_w + 2000, 3500, "GROUND")
    s.text(px + b.plot_w / 2.0, py - 2900, "ROAD", 400, "TEXT")
    s.dim_h(px, px + b.plot_w, py + b.plot_d + 900)
    s.dim_v(py, py + b.plot_d, px - 900)
    s.dim_v(py, 0, d.W / 2.0)
    s.dim_v(d.D, py + b.plot_d, d.W / 2.0)
    s.dim_h(px, 0, d.D / 2.0)
    s.dim_h(d.W, px + b.plot_w, d.D / 2.0)
    cover = 100.0 * d.W * d.D / float(b.plot_w * b.plot_d)
    s.text(px, py - 5300, "Plot %.1f sq.m   |   ground coverage %.1f %%   |   built-up %.1f sq.m"
           % (b.plot_w * b.plot_d / 1e6, cover, d.W * d.D * d.floors / 1e6), 220, "TEXT", "l")
    s.title_block(px, py - 6300, d, "1:200")
    return s


def all_sheets(d):
    sheets = [floor_plan(d, f) for f in range(d.floors)]
    sheets += [elevation(d, face) for face in ("front", "rear", "left", "right")]
    sheets += [section(d), structure_plan(d), site_plan(d)]
    return sheets
