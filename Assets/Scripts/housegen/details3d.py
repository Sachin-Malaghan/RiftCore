"""Architectural detail for the 3D model: everything beyond walls and slabs.

Exterior: window frames and sunshades, entrance porch, floor bands, parapet
coping, roof tank, compound wall with gates, paving and planting.
Interior: floor finishes, door frames with open leaves, and furniture and
fixtures for every room type.

All geometry is placed from the building model, in plan millimetres, through
the builder's box() helper.
"""
from . import model as M

FRAME = (0.30, 0.20, 0.12)
FRAME_WHITE = (0.92, 0.92, 0.90)
SHADE = (0.80, 0.80, 0.78)
BAND = (0.55, 0.52, 0.48)
COPING = (0.70, 0.70, 0.70)
COMPOUND = (0.86, 0.84, 0.78)
GATE = (0.16, 0.16, 0.18)
TANK = (0.15, 0.15, 0.17)
SHRUB = (0.22, 0.50, 0.22)

FLOOR_FINISH = {
    "living": (0.78, 0.72, 0.62), "lounge": (0.78, 0.72, 0.62), "dining": (0.78, 0.72, 0.62),
    "master": (0.62, 0.45, 0.30), "bedroom": (0.62, 0.45, 0.30), "study": (0.62, 0.45, 0.30),
    "kitchen": (0.72, 0.72, 0.70), "bath": (0.45, 0.55, 0.62), "pooja": (0.90, 0.86, 0.72),
    "garage": (0.50, 0.50, 0.50), "hall": (0.80, 0.76, 0.68), "stair": (0.80, 0.76, 0.68),
}

WOOD = (0.48, 0.31, 0.17)
WOOD_DARK = (0.30, 0.19, 0.11)
FABRIC = (0.36, 0.42, 0.52)
FABRIC_WARM = (0.62, 0.36, 0.28)
LINEN = (0.93, 0.92, 0.88)
PILLOW = (0.98, 0.98, 0.96)
STONE = (0.85, 0.84, 0.80)
STEEL = (0.75, 0.77, 0.80)
BLACK = (0.08, 0.08, 0.09)
CERAMIC = (0.96, 0.96, 0.95)
RUG = (0.55, 0.30, 0.26)
GOLD = (0.90, 0.72, 0.22)
MIRROR = (0.70, 0.82, 0.90)


MATERIALS = {
    FRAME: "wood_dark", FRAME_WHITE: "paint", SHADE: "paint", BAND: "paint", COPING: "concrete",
    COMPOUND: "brick", GATE: "metal", WOOD: "wood", WOOD_DARK: "wood_dark",
    FABRIC: "fabric", FABRIC_WARM: "fabric", LINEN: "fabric", PILLOW: "fabric", RUG: "fabric",
    STONE: "marble", STEEL: "metal", CERAMIC: "ceramic", GOLD: "metal", MIRROR: "mirror",
}

FLOOR_MATERIAL = {
    "living": "marble", "lounge": "marble", "dining": "marble", "hall": "marble", "stair": "marble",
    "pooja": "marble", "master": "wood_floor", "bedroom": "wood_floor", "study": "wood_floor",
    "kitchen": "tile", "bath": "bath_tile", "garage": "concrete",
}


def _outward(d, o):
    """(dx, dy) pointing out of the building for an opening on an external wall, else None."""
    if o.axis == "h":
        if o.pos == 0:
            return (0, -1)
        if o.pos == d.D:
            return (0, 1)
    else:
        if o.pos == 0:
            return (-1, 0)
        if o.pos == d.W:
            return (1, 0)
    return None


def _along(bld, name, parent, o, t0, t1, n0, n1, z0, z1, color, **kw):
    """Box positioned along an opening's wall: t = distance along the wall,
    n = distance across it (plan mm)."""
    if o.axis == "h":
        return bld.box(name, parent, t0, t1, o.pos + min(n0, n1), o.pos + max(n0, n1), z0, z1, color, **kw)
    return bld.box(name, parent, o.pos + min(n0, n1), o.pos + max(n0, n1), t0, t1, z0, z1, color, **kw)


# ----------------------------------------------------------------------
#  Exterior
# ----------------------------------------------------------------------

def exterior(bld, root):
    rc, d = bld.rc, bld.d
    b = d.brief
    ext = rc.spawn("empty", name="Exterior Details", parent=root)
    e = M.EXT_T / 2.0

    for f in range(d.floors):
        z = d.ffl(f)
        fl = rc.spawn("empty", name="%s Details" % d.floor_name(f), parent=ext)
        for o in d.openings_on(f):
            out = _outward(d, o)
            if not out:
                continue
            sgn = out[1] if o.axis == "h" else out[0]
            face = sgn * e                                 # outer face of the wall
            zb, zt = z + o.sill, z + o.sill + o.height
            if o.kind == "window":
                fr = 60
                for a, b2, za, zc in ((o.start - fr, o.end + fr, zt, zt + fr),
                                      (o.start - fr, o.start, zb, zt), (o.end, o.end + fr, zb, zt)):
                    _along(bld, "Window Frame", fl, o, a, b2, face, face + sgn * 40, za, zc, FRAME_WHITE)
                _along(bld, "Sunshade", fl, o, o.start - 200, o.end + 200, face, face + sgn * 450,
                       zt + 90, zt + 165, SHADE)
            elif o.kind == "door":
                _along(bld, "Door Frame", fl, o, o.start - 70, o.end + 70, face, face + sgn * 40, zt, zt + 70, FRAME)
                _along(bld, "Door Frame", fl, o, o.start - 70, o.start, face, face + sgn * 40, zb, zt, FRAME)
                _along(bld, "Door Frame", fl, o, o.end, o.end + 70, face, face + sgn * 40, zb, zt, FRAME)
                if o.room == "Main Entrance":
                    # Porch: canopy slab on two posts over the entrance step.
                    _along(bld, "Porch Canopy", fl, o, o.start - 600, o.end + 600, face, face + sgn * 1500,
                           zt + 200, zt + 320, SHADE)
                    for t in (o.start - 520, o.end + 400):
                        _along(bld, "Porch Post", fl, o, t, t + 120, face + sgn * 1300, face + sgn * 1420,
                               0, zt + 200, FRAME_WHITE)
                else:
                    _along(bld, "Sunshade", fl, o, o.start - 200, o.end + 200, face, face + sgn * 600,
                           zt + 90, zt + 165, SHADE)
            elif o.kind == "garage":
                _along(bld, "Sunshade", fl, o, o.start - 200, o.end + 200, face, face + sgn * 600,
                       zt + 90, zt + 165, SHADE)

        # Projecting band at each slab level.
        zs = z + d.H - M.SLAB_T
        p = 50
        bld.box("Floor Band", fl, -e - p, d.W + e + p, -e - p, -e, zs, zs + M.SLAB_T, BAND)
        bld.box("Floor Band", fl, -e - p, d.W + e + p, d.D + e, d.D + e + p, zs, zs + M.SLAB_T, BAND)
        bld.box("Floor Band", fl, -e - p, -e, -e, d.D + e, zs, zs + M.SLAB_T, BAND)
        bld.box("Floor Band", fl, d.W + e, d.W + e + p, -e, d.D + e, zs, zs + M.SLAB_T, BAND)

    rl = d.roof_level
    if b.style != "traditional":
        top = rl + M.PARAPET
        c = 45
        bld.box("Parapet Coping", ext, -e - c, d.W + e + c, -e - c, -e + M.INT_T + c, top, top + 50, COPING)
        bld.box("Parapet Coping", ext, -e - c, d.W + e + c, d.D + e - M.INT_T - c, d.D + e + c, top, top + 50, COPING)
        bld.box("Parapet Coping", ext, -e - c, -e + M.INT_T + c, -e, d.D + e, top, top + 50, COPING)
        bld.box("Parapet Coping", ext, d.W + e - M.INT_T - c, d.W + e + c, -e, d.D + e, top, top + 50, COPING)
        # Overhead water tank on a small plinth, towards the rear.
        tx, ty = d.W - 1700, d.D - 1700
        bld.box("Tank Stand", ext, tx - 600, tx + 600, ty - 600, ty + 600, rl, rl + 500, COPING)
        rc.spawn("cylinder", name="Water Tank", parent=ext,
                 position=(bld.wx(tx), (rl + 500 + 650) / 1000.0, bld.wz(ty)),
                 scale=(1.1, 1.3, 1.1), color=TANK, roughness=0.6)
        bld.count += 1
    else:
        oh = M.ROOF_OVERHANG
        ridge = rl + d.ridge_height
        bld.box("Ridge Cap", ext, -oh, d.W + oh, d.D / 2.0 - 90, d.D / 2.0 + 90, ridge - 40, ridge + 60, WOOD_DARK)
        for y0, y1 in ((-oh - 40, -oh + 60), (d.D + oh - 60, d.D + oh + 40)):
            bld.box("Fascia", ext, -oh, d.W + oh, y0, y1, rl - 60, rl + 110, FRAME_WHITE)

    _site(bld, root)


def _site(bld, root):
    rc, d = bld.rc, bld.d
    b = d.brief
    site = rc.spawn("empty", name="Compound", parent=root)
    side = max(b.setback_side, (b.plot_w - d.W) / 2.0)
    x0, y0 = -side, -b.setback_front
    x1, y1 = x0 + b.plot_w, y0 + b.plot_d
    t, h = 115, 1500

    # Gates: one in front of the main door, one in front of the garage.
    gaps = []
    door = next((o for o in d.openings if o.room == "Main Entrance"), None)
    if door:
        gaps.append((door.start - 250, door.end + 250, "Pedestrian Gate"))
    for o in d.openings:
        if o.kind == "garage":
            gaps.append((o.start - 100, o.end + 100, "Vehicle Gate"))
    gaps.sort()
    cur = x0
    for g0, g1, name in gaps:
        g0, g1 = max(g0, x0 + 300), min(g1, x1 - 300)
        if g0 > cur:
            bld.box("Compound Wall", site, cur, g0, y0, y0 + t, 0, h, COMPOUND)
        for px in (g0 - 230, g1):
            bld.box("Gate Pillar", site, px, px + 230, y0 - 60, y0 + 230, 0, h + 300, COMPOUND)
        bld.box(name, site, g0 + 30, g1 - 30, y0 + 30, y0 + 70, 120, h - 100, GATE, metallic=0.6, roughness=0.4)
        cur = g1
    bld.box("Compound Wall", site, cur, x1, y0, y0 + t, 0, h, COMPOUND)
    bld.box("Compound Wall", site, x0, x1, y1 - t, y1, 0, h, COMPOUND)
    bld.box("Compound Wall", site, x0, x0 + t, y0, y1, 0, h, COMPOUND)
    bld.box("Compound Wall", site, x1 - t, x1, y0, y1, 0, h, COMPOUND)

    # Road in front of the plot, with a kerb.
    bld.box("Road", site, x0 - 12000, x1 + 12000, y0 - 7500, y0 - 1200, 0, 25, (0.20, 0.20, 0.21),
            material="concrete", roughness=0.9)
    bld.box("Footpath", site, x0 - 12000, x1 + 12000, y0 - 1200, y0, 0, 120, (0.6, 0.6, 0.6), material="paving")
    # Paved apron around the house and planting along the front.
    a = 600
    e = M.EXT_T / 2.0
    bld.box("Apron", site, -e - a, d.W + e + a, -e - a, d.D + e + a, 20, 50, (0.58, 0.57, 0.55))
    occupied = [(g0 - 400, g1 + 400) for g0, g1, _ in gaps]
    n = max(2, int(d.W // 1800))
    for i in range(n):
        sx = (i + 0.5) * d.W / n
        if any(a0 <= sx <= a1 for a0, a1 in occupied):
            continue
        rc.spawn("sphere", name="Shrub", parent=site, position=(bld.wx(sx), 0.35, bld.wz(-b.setback_front + 700)),
                 scale=(0.9, 0.7, 0.9), color=SHRUB, roughness=0.9)
        bld.count += 1


# ----------------------------------------------------------------------
#  Interior
# ----------------------------------------------------------------------

class _RoomFrame:
    """Room-local coordinates: u across the room (plan X), v from the hall
    wall (0) to the far wall, in clear millimetres."""

    def __init__(self, bld, d, room, parent, z):
        self.bld, self.parent, self.z = bld, parent, z
        self.front = room.y == 0
        self.x0 = room.x + (M.EXT_T if room.x == 0 else M.INT_T) / 2.0
        self.x1 = room.x2 - (M.EXT_T if room.x2 == d.W else M.INT_T) / 2.0
        ya = room.y + (M.EXT_T if room.y == 0 else M.INT_T) / 2.0
        yb = room.y2 - (M.EXT_T if room.y2 == d.D else M.INT_T) / 2.0
        self.ya, self.yb = ya, yb
        self.w = self.x1 - self.x0
        self.d = yb - ya

    def y(self, v):
        return self.yb - v if self.front else self.ya + v

    def box(self, name, u0, u1, v0, v1, z0, z1, color, **kw):
        ys = sorted((self.y(v0), self.y(v1)))
        return self.bld.box(name, self.parent, self.x0 + u0, self.x0 + u1, ys[0], ys[1],
                            self.z + z0, self.z + z1, color, **kw)

    def prop(self, kind, name, u, v, size, rot=0.0, color=(1, 1, 1), lift=None):
        bld = self.bld
        bld.count += 1
        h = size[1] if isinstance(size, tuple) else size
        y = (self.z + (h * 500.0 if lift is None else lift)) / 1000.0
        return bld.rc.spawn(kind, name=name, parent=self.parent,
                            position=(bld.wx(self.x0 + u), y, bld.wz(self.y(v))),
                            rotation=(0, rot, 0), scale=size, color=color)

    def facing(self, towards_far):
        """Y rotation for a prop that should face the far wall (True) or the hall."""
        looks_plus_y = towards_far != self.front        # far wall is at +Y in the rear band
        return 180.0 if looks_plus_y else 0.0


def _bed(r, width, name):
    w, d = r.w, r.d
    if w < width + 700 or d < 2700:
        width = min(width, w - 500)
        if width < 900:
            return
    u0 = (w - width) / 2.0
    v1 = d - 60
    v0 = v1 - 2050
    r.box("Rug", u0 - 350, u0 + width + 350, v0 - 500, v1 - 500, 15, 30, RUG, roughness=0.95)
    r.box("%s Frame" % name, u0, u0 + width, v0, v1, 0, 320, WOOD_DARK)
    r.box("Mattress", u0 + 30, u0 + width - 30, v0 + 30, v1 - 90, 320, 520, LINEN, roughness=0.9)
    r.box("Blanket", u0 + 30, u0 + width - 30, v0 + 30, v0 + 1150, 520, 550, FABRIC, roughness=0.95)
    r.box("Headboard", u0 - 40, u0 + width + 40, v1 - 80, v1, 0, 1050, WOOD)
    half = width / 2.0
    for k in range(2 if width >= 1400 else 1):
        pu = u0 + 120 + k * half if width >= 1400 else u0 + width / 2.0 - 250
        r.box("Pillow", pu, pu + min(500, half - 200), v1 - 520, v1 - 150, 520, 620, PILLOW, roughness=0.95)
    for su in (u0 - 480, u0 + width + 30):
        if su > 60 and su + 450 < w - 60:
            r.box("Side Table", su, su + 450, v1 - 470, v1 - 20, 0, 480, WOOD)
    # Wardrobe on the side wall away from the door.
    if w > width + 1500 and d > 3000:
        r.box("Wardrobe", w - 640, w - 40, 300, min(2100, d - 2300), 0, 2100, WOOD)
        r.box("Wardrobe Doors", w - 660, w - 640, 330, min(2070, d - 2330), 60, 2060, FRAME_WHITE)


def _seating(r, name="Sofa"):
    w, d = r.w, r.d
    if w < 2600 or d < 2400:
        return
    L = min(2200, d - 1200)
    v0 = (d - L) / 2.0 + 200
    u1 = w - 80
    r.box("Rug", w / 2.0 - 1000, w / 2.0 + 1000, d / 2.0 - 700, d / 2.0 + 900, 15, 30, RUG, roughness=0.95)
    r.box("%s Base" % name, u1 - 900, u1, v0, v0 + L, 0, 420, FABRIC, roughness=0.95)
    r.box("%s Back" % name, u1 - 220, u1, v0, v0 + L, 420, 820, FABRIC, roughness=0.95)
    r.box("%s Arm" % name, u1 - 900, u1, v0, v0 + 180, 420, 620, FABRIC, roughness=0.95)
    r.box("%s Arm" % name, u1 - 900, u1, v0 + L - 180, v0 + L, 420, 620, FABRIC, roughness=0.95)
    r.box("Coffee Table", u1 - 1900, u1 - 1300, v0 + L / 2.0 - 450, v0 + L / 2.0 + 450, 0, 400, WOOD_DARK)
    if w > 3600:
        r.box("TV Unit", 60, 480, v0 + 200, v0 + L - 200, 0, 480, WOOD)
        r.box("Television", 80, 130, v0 + 450, v0 + L - 450, 850, 1550, BLACK, metallic=0.5, roughness=0.2)
    if w > 4400:
        r.box("Armchair Base", w / 2.0 - 1100, w / 2.0 - 300, d - 1000, d - 200, 0, 420, FABRIC_WARM, roughness=0.95)
        r.box("Armchair Back", w / 2.0 - 1100, w / 2.0 - 300, d - 380, d - 200, 420, 800, FABRIC_WARM, roughness=0.95)
    r.prop("model:tree_round", "Indoor Plant", w - 350, 350, 1.1)


def _dining(r):
    w, d = r.w, r.d
    if w < 2200 or d < 2400:
        return
    seats = 3 if w > 3300 else 2
    L, B = (1800 if seats == 3 else 1300), 850
    u0, v0 = (w - L) / 2.0, (d - B) / 2.0
    r.box("Dining Table Top", u0, u0 + L, v0, v0 + B, 710, 760, WOOD)
    for lu in (u0 + 60, u0 + L - 130):
        for lv in (v0 + 60, v0 + B - 130):
            r.box("Table Leg", lu, lu + 70, lv, lv + 70, 0, 710, WOOD_DARK)
    for i in range(seats):
        cu = u0 + (i + 0.5) * L / seats
        r.prop("model:chair", "Dining Chair", cu, v0 - 260, 0.9, r.facing(True))
        r.prop("model:chair", "Dining Chair", cu, v0 + B + 260, 0.9, r.facing(False))
    if w > 3000:
        r.box("Sideboard", w - 480, w - 60, d / 2.0 - 700, d / 2.0 + 700, 0, 850, WOOD_DARK)


def _kitchen(r):
    w, d = r.w, r.d
    if w < 1800 or d < 2200:
        return
    v0, v1 = 250, d - 60
    r.box("Base Cabinets", 60, 660, v0, v1, 0, 850, WOOD)
    r.box("Countertop", 60, 690, v0, v1, 850, 900, STONE, roughness=0.3)
    r.box("Sink", 180, 580, v0 + 500, v0 + 1100, 895, 905, STEEL, metallic=0.9, roughness=0.25)
    r.box("Hob", 160, 600, v1 - 1300, v1 - 600, 900, 915, BLACK, metallic=0.4, roughness=0.3)
    r.box("Chimney Hood", 60, 500, v1 - 1350, v1 - 550, 1750, 2000, STEEL, metallic=0.8, roughness=0.3)
    r.box("Wall Cabinets", 60, 400, v0, v1 - 1500, 1500, 2150, WOOD)
    if w > 2500:
        r.box("Refrigerator", w - 760, w - 60, d / 2.0 - 350, d / 2.0 + 350, 0, 1800, STEEL, metallic=0.7, roughness=0.35)
    if w > 3000:
        r.box("Tall Unit", w - 660, w - 60, d / 2.0 + 420, min(d / 2.0 + 1220, d - 1100), 0, 2100, WOOD)


def _bath(r):
    w, d = r.w, r.d
    if w < 1000 or d < 1800:
        return
    v1 = d - 60
    r.box("WC Pan", 180, 580, v1 - 700, v1 - 180, 0, 400, CERAMIC, roughness=0.2)
    r.box("WC Cistern", 180, 580, v1 - 200, v1, 400, 820, CERAMIC, roughness=0.2)
    bu = max(700, w - 620)
    r.box("Vanity", bu, bu + 560, v1 - 460, v1, 0, 800, WOOD)
    r.box("Basin", bu + 40, bu + 520, v1 - 430, v1 - 40, 800, 880, CERAMIC, roughness=0.2)
    r.box("Mirror", bu + 40, bu + 520, v1 - 30, v1, 1100, 1800, MIRROR, metallic=0.9, roughness=0.05)
    if d > 2600:
        r.box("Shower Tray", 80, w - 80, v1 - 1900, v1 - 950, 15, 60, STONE, roughness=0.4)
        r.box("Shower Screen", 80, w - 80, v1 - 950, v1 - 930, 60, 2000, MIRROR, metallic=0.6, roughness=0.1)


def _pooja(r):
    w, d = r.w, r.d
    if w < 900:
        return
    u0, v1 = (w - 800) / 2.0, d - 60
    r.box("Altar", u0, u0 + 800, v1 - 450, v1, 0, 850, WOOD)
    r.box("Altar Shelf", u0 + 100, u0 + 700, v1 - 300, v1, 850, 1150, WOOD_DARK)
    r.box("Idol", u0 + 300, u0 + 500, v1 - 230, v1 - 70, 1150, 1500, GOLD, metallic=0.9, roughness=0.3)
    r.box("Lamp", u0 + 120, u0 + 220, v1 - 400, v1 - 300, 850, 1000, GOLD, metallic=0.9, roughness=0.3)
    r.box("Mat", u0 - 50, u0 + 850, v1 - 1500, v1 - 650, 15, 30, FABRIC_WARM, roughness=0.95)


def _study(r):
    w, d = r.w, r.d
    if w < 1900 or d < 2200:
        return
    L = min(1500, w - 500)
    u0, v1 = (w - L) / 2.0, d - 60
    r.box("Desk Top", u0, u0 + L, v1 - 680, v1, 720, 760, WOOD)
    for lu in (u0 + 30, u0 + L - 90):
        r.box("Desk Leg", lu, lu + 60, v1 - 650, v1 - 30, 0, 720, WOOD_DARK)
    r.box("Monitor", u0 + L / 2.0 - 300, u0 + L / 2.0 + 300, v1 - 180, v1 - 140, 800, 1200, BLACK, metallic=0.5, roughness=0.2)
    r.prop("model:chair", "Desk Chair", u0 + L / 2.0, v1 - 1050, 0.9, r.facing(True))
    if w > 2600:
        r.box("Bookshelf", w - 400, w - 60, 400, min(1900, d - 1200), 0, 2000, WOOD_DARK)
        for k in range(4):
            r.box("Books", w - 380, w - 120, 450, min(1850, d - 1250), 120 + k * 470, 380 + k * 470,
                  ((0.6, 0.25, 0.2), (0.2, 0.35, 0.6), (0.25, 0.5, 0.3), (0.7, 0.6, 0.25))[k])


def _garage(r):
    if r.w < 2600 or r.d < 4600:
        return
    r.prop("model:car", "Car", r.w / 2.0, r.d / 2.0, 4.2, 90.0, lift=4.2 * 300.0)
    r.box("Workbench", r.w - 560, r.w - 60, 200, 1500, 0, 880, WOOD_DARK)


FURNISH = {
    "master": lambda r: _bed(r, 1800, "King Bed"),
    "bedroom": lambda r: _bed(r, 1500, "Queen Bed"),
    "living": lambda r: _seating(r, "Sofa"),
    "lounge": lambda r: _seating(r, "Lounge Sofa"),
    "dining": _dining, "kitchen": _kitchen, "bath": _bath, "pooja": _pooja,
    "study": _study, "garage": _garage,
}


def interior(bld, floor_parent, floor):
    """Floor finishes, door frames with open leaves and furniture for one floor."""
    rc, d = bld.rc, bld.d
    z = d.ffl(floor)
    finishes = rc.spawn("empty", name="Floor Finishes", parent=floor_parent)
    furniture = rc.spawn("empty", name="Furniture", parent=floor_parent)

    for room in d.rooms_on(floor):
        if room.kind == "stair" and floor < d.floors - 1:
            continue                                    # open stairwell
        frame = _RoomFrame(bld, d, room, finishes, z)
        frame.box("%s Floor" % room.name, 0, frame.w, 0, frame.d, 0, 18,
                  FLOOR_FINISH.get(room.kind, (0.8, 0.78, 0.72)), roughness=0.5,
                  material=FLOOR_MATERIAL.get(room.kind, "tile"))
        if room.kind in FURNISH:
            grp = rc.spawn("empty", name="%s Furniture" % room.name, parent=furniture)
            FURNISH[room.kind](_RoomFrame(bld, d, room, grp, z))


def door_leaf(bld, parent, w, o, z, exterior):
    """A door: frame head and jambs, with the leaf closed on external doors
    and standing open on internal ones so the rooms read as connected."""
    zt = z + o.height
    t = w.t / 2.0
    if w.axis != "h":
        return
    y = w.pos
    for a, b in ((o.start, o.start + 45), (o.end - 45, o.end)):
        bld.box("Door Jamb", parent, a, b, y - t - 12, y + t + 12, z, zt, FRAME)
    bld.box("Door Head", parent, o.start, o.end, y - t - 12, y + t + 12, zt - 45, zt, FRAME)
    if exterior:
        bld.box("Door %s" % o.tag, parent, o.start + 45, o.end - 45, y - 22, y + 22, z, zt - 45, WOOD)
        bld.box("Door Handle", parent, o.end - 170, o.end - 110, y - 60, y + 60, z + 950, z + 1050, STEEL,
                metallic=0.9, roughness=0.3)
    else:
        leaf = o.width - 90
        y0, y1 = sorted((y, y + o.swing * leaf))
        bld.box("Door %s" % o.tag, parent, o.start + 45, o.start + 85, y0, y1, z, zt - 45, WOOD)
