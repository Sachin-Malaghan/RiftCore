"""The building model: one coordinated description of the house that every
drawing, schedule and the 3D scene are produced from.

Plan coordinates are millimetres, X across the front, Y from the front wall
(y = 0, facing the road) to the rear. Rooms are laid out on wall centre
lines in three bands - front rooms, a central hall, rear rooms - which keeps
every wall continuous from floor to floor so the structure stacks.
"""
import math
from dataclasses import dataclass, field

from . import vastu as V

EXT_T = 230          # external wall thickness
INT_T = 115          # internal wall thickness
SLAB_T = 125         # floor / roof slab
PLINTH = 450         # finished ground floor above ground level
LINTEL = 2100        # door / window head above floor
PARAPET = 900
HALL = 1200          # central hall width (centre to centre)
COLUMN = 230
ROOF_PITCH_DEG = 25.0
ROOF_OVERHANG = 450
FOOTING_DEPTH = 1500
TREAD = 270
FLIGHT_W = 1000


def snap(v, module=50):
    return int(round(v / float(module))) * module


# kind: (target area m2, fixed width mm or 0, zone)
ROOM_SPECS = {
    "living":  (22.0, 0, "front"),
    "dining":  (12.0, 0, "back"),
    "kitchen": (10.5, 0, "back"),
    "master":  (16.5, 0, "front"),
    "bedroom": (12.5, 0, "back"),
    "bath":    (4.6, 1650, "back"),
    "study":   (9.0, 0, "front"),
    "pooja":   (3.0, 1500, "back"),
    "garage":  (19.0, 3300, "front"),
    "stair":   (0.0, 2300, "back"),
    "lounge":  (14.0, 0, "front"),
    "hall":    (0.0, 0, "hall"),
}

# kind: (width, height, sill)
WINDOW_SPECS = {
    "living": (1800, 1500, 600), "lounge": (1800, 1500, 600),
    "master": (1500, 1350, 900), "bedroom": (1500, 1350, 900), "study": (1200, 1350, 900),
    "dining": (1500, 1350, 900), "kitchen": (1200, 1050, 1050),
    "bath": (600, 600, 1500), "pooja": (600, 600, 1500), "garage": (900, 600, 1500),
    "stair": (900, 1200, 1200), "hall": (600, 1200, 900),
}

DOOR_WIDTH = {"bath": 750, "pooja": 750, "garage": 900}

_BASE_SPECS = dict(ROOM_SPECS)


@dataclass
class Room:
    name: str
    kind: str
    floor: int
    x: int
    y: int
    w: int
    d: int
    clear_w: int = 0
    clear_d: int = 0

    @property
    def x2(self):
        return self.x + self.w

    @property
    def y2(self):
        return self.y + self.d

    @property
    def area(self):
        return self.clear_w * self.clear_d / 1e6


@dataclass
class Wall:
    floor: int
    axis: str        # "h": runs along X at y = pos;  "v": runs along Y at x = pos
    pos: int
    a: int
    b: int
    exterior: bool

    @property
    def t(self):
        return EXT_T if self.exterior else INT_T

    @property
    def length(self):
        return self.b - self.a


@dataclass
class Opening:
    kind: str        # "door", "window", "opening" (no leaf), "garage"
    floor: int
    axis: str
    pos: int
    start: int
    width: int
    height: int
    sill: int
    room: str
    swing: int = 1   # doors: side of the wall the leaf opens to (+1 / -1 along the normal)
    tag: str = ""

    @property
    def end(self):
        return self.start + self.width


@dataclass
class Stair:
    x: int
    y: int
    w: int
    d: int
    risers: int
    riser_h: float
    tread: int
    flight_w: int
    y0: int          # first riser line
    landing_y: int   # start of the mid landing
    dir: int = 1     # +1: entered from low Y (rear band), -1: from high Y (front band)


@dataclass
class Design:
    brief: object
    W: int = 0
    D: int = 0
    df: int = 0                      # depth of the front band
    rooms: list = field(default_factory=list)
    walls: list = field(default_factory=list)
    openings: list = field(default_factory=list)
    columns: list = field(default_factory=list)   # (x, y)
    stair: object = None
    footing: int = 1200
    notes: list = field(default_factory=list)

    # ---- levels (mm above ground) ----
    @property
    def H(self):
        return self.brief.floor_height

    @property
    def floors(self):
        return self.brief.floors

    def ffl(self, floor):
        return PLINTH + floor * self.H

    @property
    def roof_level(self):
        return PLINTH + self.floors * self.H

    @property
    def ridge_height(self):
        return int(round((self.D / 2.0 + ROOF_OVERHANG) * math.tan(math.radians(ROOF_PITCH_DEG))))

    @property
    def top_level(self):
        if self.brief.style == "traditional":
            return self.roof_level + self.ridge_height
        return self.roof_level + PARAPET

    @property
    def y_hall(self):
        return self.df

    @property
    def y_back(self):
        return self.df + HALL

    def rooms_on(self, floor):
        return [r for r in self.rooms if r.floor == floor]

    def openings_on(self, floor, axis=None, pos=None):
        return [o for o in self.openings if o.floor == floor
                and (axis is None or o.axis == axis) and (pos is None or o.pos == pos)]

    def floor_name(self, floor):
        return ["Ground Floor", "First Floor", "Second Floor"][floor] if floor < 3 else "Floor %d" % floor


# ----------------------------------------------------------------------
#  Room program
# ----------------------------------------------------------------------

def _program(brief):
    """Rooms per floor as (front list, back list) of (name, kind)."""
    F, B, T = brief.floors, brief.bedrooms, brief.bathrooms
    ground_beds = B if F == 1 else (1 if B >= 3 else 0)
    upper_beds = B - ground_beds
    ground_baths = T if F == 1 else 1
    upper_baths = 0 if F == 1 else max(1, T - 1)

    floors = []
    # ---- ground ----
    front, back = [], []
    if brief.garage:
        front.append(("Garage", "garage"))
    front.append(("Living Room", "living"))
    if brief.study:
        front.append(("Study", "study"))
    if F > 1:
        back.append(("Staircase", "stair"))
    back.append(("Kitchen", "kitchen"))
    back.append(("Dining", "dining"))
    if brief.pooja:
        back.append(("Pooja", "pooja"))
    beds = []
    for i in range(ground_beds):
        master = (F == 1 and i == 0)
        beds.append(("Master Bedroom" if master else "Bedroom %d" % (i + 1 if F > 1 else i + 1),
                     "master" if master else "bedroom"))
    _place_private(front, back, beds, ground_baths, 0)
    floors.append((front, back))

    # ---- upper floors ----
    for f in range(1, F):
        share = upper_beds // (F - 1) + (1 if (f - 1) < upper_beds % (F - 1) else 0)
        baths = upper_baths // (F - 1) + (1 if (f - 1) < upper_baths % (F - 1) else 0)
        front, back = [], [("Staircase", "stair")]
        beds = []
        first_no = ground_beds + sum(
            upper_beds // (F - 1) + (1 if (g - 1) < upper_beds % (F - 1) else 0) for g in range(1, f))
        for i in range(share):
            master = (f == 1 and i == 0)
            beds.append(("Master Bedroom" if master else "Bedroom %d" % (first_no + i + 1),
                         "master" if master else "bedroom"))
        if not beds or share < 2:
            front.append(("Family Lounge", "lounge"))
        _place_private(front, back, beds, max(1, baths), f)
        floors.append((front, back))
    return floors


def _place_private(front, back, beds, baths, floor):
    """Bedrooms alternate between the bands (master to the front); each
    bathroom sits beside a bedroom in the rear band."""
    bath_no = 0
    for i, bed in enumerate(beds):
        to_front = (bed[1] == "master") or (i % 2 == 0 and len(front) < 3)
        if to_front:
            front.append(bed)
        else:
            if bath_no < baths:
                bath_no += 1
                back.append(("Bath %d" % bath_no if baths > 1 else "Bath", "bath"))
            back.append(bed)
    while bath_no < baths:
        bath_no += 1
        back.append(("Bath %d" % bath_no if baths > 1 else "Bath", "bath"))


def _program_vastu(brief):
    """Room program zoned by compass corner (see vastu.py)."""
    F, B, T = brief.floors, brief.bedrooms, brief.bathrooms
    ground_beds = B if F == 1 else (1 if B >= 3 else 0)
    upper_beds = B - ground_beds
    ground_baths = T if F == 1 else 1
    upper_baths = 0 if F == 1 else max(1, T - 1)
    floors = []

    def lighter(bands):
        fa = sum(_area(k) for _, k in bands.items("front"))
        ba = sum(_area(k) for _, k in bands.items("back"))
        return "front" if fa <= ba else "back"

    def finish(bands):
        # Every band needs a room that can stretch, and the two bands should
        # carry comparable areas; a study fills in where the brief leaves a gap.
        for band, other in (("front", "back"), ("back", "front")):
            mine = [k for _, k in bands.items(band) if not ROOM_SPECS[k][1]]
            a = sum(_area(k) for k in mine)
            o = sum(_area(k) for _, k in bands.items(other) if not ROOM_SPECS[k][1])
            if not mine or a < 0.5 * o:
                bands.middle(band, ("Study", "study"))
        floors.append((bands.items("front"), bands.items("back")))

    # ---- ground ----
    g = V.Bands(brief.facing)
    if F > 1:
        g.corner("SW", ("Staircase", "stair"))
    kitchen_band = g.corner("SE", ("Kitchen", "kitchen"))
    g.corner("NE", ("Pooja", "pooja"))
    for i in range(ground_baths):
        g.corner("NW", ("Bath %d" % (i + 1) if ground_baths > 1 else "Bath", "bath"))
    if brief.garage:
        g.front_outer(("Garage", "garage"))
    g.middle("front", ("Living Room", "living"))
    if kitchen_band == "front":
        g.middle("back", ("Dining", "dining"))
    else:
        g.corner("SE", ("Dining", "dining"))
    if brief.study:
        g.middle(lighter(g), ("Study", "study"))
    for i in range(ground_beds):
        if F == 1 and i == 0:
            g.corner("SW", ("Master Bedroom", "master"))
        else:
            g.middle(lighter(g), ("Bedroom %d" % (i + 1), "bedroom"))
    finish(g)

    # ---- upper floors ----
    bed_no = ground_beds
    for f in range(1, F):
        share = upper_beds // (F - 1) + (1 if (f - 1) < upper_beds % (F - 1) else 0)
        baths = max(1, upper_baths // (F - 1) + (1 if (f - 1) < upper_baths % (F - 1) else 0))
        u = V.Bands(brief.facing)
        u.corner("SW", ("Staircase", "stair"))
        for i in range(share):
            bed_no += 1
            if f == 1 and i == 0:
                u.corner("SW", ("Master Bedroom", "master"))
        u.corner("NE", ("Family Lounge", "lounge"))
        for i in range(baths):
            u.corner("NW", ("Bath %d" % (i + 1) if baths > 1 else "Bath", "bath"))
        first = bed_no - share
        for i in range(share):
            if f == 1 and i == 0:
                continue
            u.middle(lighter(u), ("Bedroom %d" % (first + i + 1), "bedroom"))
        finish(u)
    return floors


def _area(kind):
    return ROOM_SPECS[kind][0]


def _band_width(items, depth):
    """Width a band needs for its rooms at their target areas."""
    total = 0.0
    for _, kind in items:
        fixed = ROOM_SPECS[kind][1]
        total += fixed if fixed else max(2700.0, _area(kind) * 1e6 / depth)
    return total


def _layout_band(items, floor, y, depth, W):
    """Rooms of one band, left to right, filling the full width W."""
    fixed_total = sum(ROOM_SPECS[k][1] for _, k in items)
    flex = [(n, k) for n, k in items if not ROOM_SPECS[k][1]]
    flex_area = sum(_area(k) for _, k in flex) or 1.0
    remaining = W - fixed_total
    rooms, x = [], 0
    flex_left = len(flex)
    for name, kind in items:
        fixed = ROOM_SPECS[kind][1]
        if fixed:
            w = fixed
        else:
            flex_left -= 1
            w = snap(remaining * _area(kind) / flex_area)
        rooms.append(Room(name, kind, floor, x, y, w, depth))
        x += w
    # The last flexible room absorbs rounding so the band closes exactly on W.
    slack = W - x
    if slack:
        for i in range(len(rooms) - 1, -1, -1):
            if not ROOM_SPECS[rooms[i].kind][1]:
                rooms[i].w += slack
                for later in rooms[i + 1:]:
                    later.x += slack
                break
    return rooms


# ----------------------------------------------------------------------
#  Openings
# ----------------------------------------------------------------------

class _WallSlots:
    """Tracks what is already cut into each wall so openings never overlap."""

    def __init__(self):
        self.used = {}

    def place(self, key, lo, hi, width, prefer="center", margin=250, gap=300):
        """Start of a free span of `width` inside [lo, hi], or None."""
        lo, hi = lo + margin, hi - margin
        if hi - lo < width:
            return None
        taken = sorted(self.used.get(key, []))
        if prefer == "start":
            candidates = [lo, (lo + hi - width) // 2, hi - width]
        elif prefer == "end":
            candidates = [hi - width, (lo + hi - width) // 2, lo]
        else:
            candidates = [(lo + hi - width) // 2, lo, hi - width]
        free, cur = [], lo
        for s, e in taken:
            if s - gap > cur:
                free.append((cur, s - gap))
            cur = max(cur, e + gap)
        if cur < hi:
            free.append((cur, hi))
        for c in candidates:
            c = snap(c, 25)
            for s, e in free:
                if c >= s and c + width <= e:
                    self.used.setdefault(key, []).append((c, c + width))
                    return c
        for s, e in free:                        # anywhere it fits
            if e - s >= width:
                c = snap((s + e - width) / 2.0, 25)
                c = min(max(c, s), e - width)
                self.used.setdefault(key, []).append((c, c + width))
                return c
        return None


def _add_openings(design):
    b = design.brief
    W, D = design.W, design.D
    yh, yb = design.y_hall, design.y_back
    slots = _WallSlots()
    out = design.openings

    for floor in range(design.floors):
        rooms = design.rooms_on(floor)
        front = [r for r in rooms if r.y == 0 and r.kind != "hall"]
        back = [r for r in rooms if r.y == yb]

        # --- doors from the hall into every room ---
        for r in front + back:
            is_front = r.y == 0
            pos = yh if is_front else yb
            swing = -1 if is_front else 1          # leaf opens into the room
            key = (floor, "h", pos)
            if r.kind == "stair":
                width = r.w - 500
                s = slots.place(key, r.x, r.x2, width)
                if s is not None:
                    out.append(Opening("opening", floor, "h", pos, s, width, design.H - SLAB_T, 0, r.name))
                continue
            if r.kind in ("living", "lounge") and r.w >= 2700:
                s = slots.place(key, r.x, r.x2, 1800)
                if s is not None:
                    out.append(Opening("opening", floor, "h", pos, s, 1800, LINTEL, 0, r.name))
                    continue
            width = DOOR_WIDTH.get(r.kind, 900)
            s = slots.place(key, r.x, r.x2, width, prefer="start")
            if s is not None:
                out.append(Opening("door", floor, "h", pos, s, width, LINTEL, 0, r.name, swing))

        # --- external doors (ground floor) ---
        if floor == 0:
            for r in front:
                if r.kind == "garage":
                    s = slots.place((0, "h", 0), r.x, r.x2, 2700)
                    if s is not None:
                        out.append(Opening("garage", 0, "h", 0, s, 2700, 2400, 0, r.name))
            living = next((r for r in front if r.kind == "living"), front[0])
            prefer = V.entrance_prefer(b.facing) if b.vastu else "start"
            s = slots.place((0, "h", 0), living.x, living.x2, 1050, prefer=prefer, margin=400)
            if s is not None:
                out.append(Opening("door", 0, "h", 0, s, 1050, LINTEL, 0, "Main Entrance", 1))
            kitchen = next((r for r in back if r.kind == "kitchen"), None)
            if kitchen:
                s = slots.place((0, "h", D), kitchen.x, kitchen.x2, 900, prefer="end")
                if s is not None:
                    out.append(Opening("door", 0, "h", D, s, 900, LINTEL, 0, "Rear Door", -1))

        # --- windows on every external wall of every room ---
        for r in rooms:
            ww, wh, sill = WINDOW_SPECS.get(r.kind, (1200, 1200, 900))
            edges = []
            if r.y == 0:
                edges.append(("h", 0, r.x, r.x2))
            if r.y2 == D:
                edges.append(("h", D, r.x, r.x2))
            if r.x == 0:
                edges.append(("v", 0, r.y, r.y2))
            if r.x2 == W:
                edges.append(("v", W, r.y, r.y2))
            for axis, pos, lo, hi in edges:
                key = (floor, axis, pos)
                for width in (ww, 1200, 900, 600):
                    if width > ww:
                        continue
                    s = slots.place(key, lo, hi, width)
                    if s is not None:
                        out.append(Opening("window", floor, axis, pos, s, width, wh, sill, r.name))
                        break

    # --- tags: one mark per distinct size ---
    marks = {}
    for kind, prefix in (("door", "D"), ("garage", "GD"), ("window", "W")):
        sizes = sorted({(o.width, o.height) for o in out if o.kind == kind}, reverse=True)
        for i, size in enumerate(sizes):
            marks[(kind, size)] = "%s%d" % (prefix, i + 1) if len(sizes) > 1 or kind != "garage" else prefix
    for o in out:
        o.tag = marks.get((o.kind, (o.width, o.height)), "")
        if o.room == "Main Entrance":
            o.tag = "MD"
    if any(o.tag == "MD" for o in out):
        b.notes.append("MD = main entrance door 1050 x %d." % LINTEL)


# ----------------------------------------------------------------------
#  Structure
# ----------------------------------------------------------------------

def _add_columns(design):
    """RCC columns on the four longitudinal wall lines, at room corners,
    thinned so bays are not shorter than about 2.7 m."""
    W, D = design.W, design.D
    ground = design.rooms_on(0)
    front_x = sorted({r.x for r in ground if r.y == 0 and r.kind != "hall"} | {W})
    back_x = sorted({r.x for r in ground if r.y == design.y_back} | {W})

    def thin(xs):
        keep = [0]
        for x in xs:
            if x in (0, W):
                continue
            if x - keep[-1] >= 2700 and W - x >= 2300:
                keep.append(x)
        keep.append(W)
        return keep

    cols = set()
    for y, xs in ((0, front_x), (design.y_hall, front_x), (design.y_back, back_x), (D, back_x)):
        for x in thin(xs):
            cols.add((x, y))
    design.columns = sorted(cols, key=lambda p: (p[1], p[0]))
    design.footing = 1200 if design.floors == 1 else 1500 if design.floors == 2 else 1800


def _add_stair(design):
    room = next((r for r in design.rooms if r.kind == "stair" and r.floor == 0), None)
    if not room:
        return
    risers = int(math.ceil(design.H / 170.0))
    if risers % 2:
        risers += 1
    going = (risers // 2 - 1) * TREAD
    # Flights start at the hall side of the stair room.
    direction = 1 if room.y >= design.y_back else -1
    y0 = room.y + 300 if direction > 0 else room.y2 - 300
    design.stair = Stair(room.x, room.y, room.w, room.d, risers, design.H / float(risers),
                         TREAD, FLIGHT_W, y0, y0 + direction * going, direction)


# ----------------------------------------------------------------------
#  Entry point
# ----------------------------------------------------------------------

def design_house(brief):
    """Lays out the whole house for a Brief and returns the Design."""
    d = Design(brief=brief)
    scale, merged = 1.0, False

    # Fit the programme to the plot. When it does not fit, do what a designer
    # would, one step at a time, and record each step as a design note.
    for attempt in range(10):
        ROOM_SPECS.update(_BASE_SPECS)
        program = _program_vastu(brief) if brief.vastu else _program(brief)
        if merged:
            program[0] = tuple(
                [("Kitchen / Dining", k) if k == "kitchen" else (n, k) for n, k in band if k != "dining"]
                for band in program[0])
        stair_front = any(k == "stair" for _, k in program[0][0])

        side, front_sb, rear_sb = brief.setback_side, brief.setback_front, brief.setback_rear
        max_w = brief.plot_w - 2 * side if brief.plot_w else 10 ** 9
        max_d = brief.plot_d - front_sb - rear_sb if brief.plot_d else 10 ** 9
        max_d = (max_d // 50) * 50

        # Band depths: the front band must take a car if there is a garage,
        # the band with the dog-leg stair must be deep enough for it.
        min_front = 5600 if brief.garage else 4000 if stair_front else 3600
        min_back = 4000 if (brief.floors > 1 and not stair_front) else 3300
        gf_front = sum(_area(k) for _, k in program[0][0])
        gf_back = sum(_area(k) for _, k in program[0][1]) + (9.0 if brief.floors > 1 else 0.0)
        total = gf_front + gf_back
        factor = scale
        if brief.target_area_m2:
            factor *= max(0.6, min(1.8, brief.target_area_m2 / (total * 1.18)))
        if factor != 1.0:
            for kind in ("living", "dining", "kitchen", "master", "bedroom", "lounge", "study"):
                area, fixed, zone = _BASE_SPECS[kind]
                ROOM_SPECS[kind] = (area * factor, fixed, zone)
            gf_front = sum(_area(k) for _, k in program[0][0])
            gf_back = sum(_area(k) for _, k in program[0][1]) + (9.0 if brief.floors > 1 else 0.0)
            total = gf_front + gf_back

        depth = snap(math.sqrt(total * 1.18 * 1e6 / 1.25), 100)    # aim for W : D of about 1.25
        if brief.plot_w:
            depth = max(depth, snap(total * 1.18 * 1e6 / max_w, 100))   # narrow plot: go deeper
        depth = max(depth, min_front + HALL + min_back)
        depth = min(depth, max_d)
        deep_enough = depth >= min_front + HALL + min_back
        usable = depth - HALL
        df = snap(usable * gf_front / total, 100)
        df = max(min_front, min(df, usable - min_back))
        db = usable - df

        width = 0.0
        for front, back in program:
            width = max(width, _band_width(front, df), _band_width(back, db))
        width = snap(width, 100)
        if width <= max_w and deep_enough:
            break

        # ---- does not fit: escalate ----
        if brief.plot_w and (side > 1000 or front_sb > 1500 or rear_sb > 1000):
            brief.setback_side, brief.setback_front, brief.setback_rear = 1000, 1500, 1000
            d.notes.append("Tight plot: setbacks reduced to 1.5 m front and 1.0 m sides / rear "
                           "(check the local rules).")
        elif not merged and any(k == "dining" for band in program[0] for _, k in band):
            merged = True
            d.notes.append("Tight plot: dining combined with the kitchen.")
        elif scale > 0.72:
            scale -= 0.14
            d.notes.append("Tight plot: room sizes reduced to %d %% of the usual targets."
                           % round(scale * 100))
        elif brief.floors < 3:
            brief.floors += 1
            scale = 0.86
            d.notes[:] = [n for n in d.notes if "room sizes reduced" not in n]
            d.notes.append("Room sizes are 86 % of the usual targets.")
            d.notes.append("The rooms do not fit on %d floor(s) of this plot: a storey was added."
                           % (brief.floors - 1))
        else:
            d.notes.append("Plot is too small for this programme even on 3 floors; "
                           "rooms are below their targets.")
            width = snap(max_w - 49, 100)
            if not deep_enough:
                depth = min_front + HALL + min_back
                usable = depth - HALL
                df = max(min_front, min(df, usable - min_back))
                db = usable - df
            break

    if width > max_w:
        width = snap(max_w - 49, 100)
    d.W, d.D, d.df = int(width), int(depth), int(df)

    for floor, (front, back) in enumerate(program):
        d.rooms += _layout_band(front, floor, 0, df, d.W)
        d.rooms.append(Room("Hall" if floor == 0 else "Lobby", "hall", floor, 0, df, d.W, HALL))
        d.rooms += _layout_band(back, floor, df + HALL, db, d.W)

    # Clear (inside) dimensions: centre-line size less half of each bounding wall.
    for r in d.rooms:
        def half(on_edge):
            return EXT_T / 2.0 if on_edge else INT_T / 2.0
        r.clear_w = int(round(r.w - half(r.x == 0) - half(r.x2 == d.W)))
        r.clear_d = int(round(r.d - half(r.y == 0) - half(r.y2 == d.D)))

    # Walls: four longitudinal lines plus the cross walls of each band.
    for floor in range(d.floors):
        for y, ext in ((0, True), (d.y_hall, False), (d.y_back, False), (d.D, True)):
            d.walls.append(Wall(floor, "h", y, 0, d.W, ext))
        d.walls.append(Wall(floor, "v", 0, 0, d.D, True))
        d.walls.append(Wall(floor, "v", d.W, 0, d.D, True))
        for r in d.rooms_on(floor):
            if r.kind != "hall" and r.x > 0:
                d.walls.append(Wall(floor, "v", r.x, r.y, r.y2, False))

    planned = sum(1 for r in d.rooms if r.kind == "bath")
    if planned != brief.bathrooms:
        d.notes.append("Bathrooms: %d planned (one per floor minimum) for %d requested."
                       % (planned, brief.bathrooms))
        brief.bathrooms = planned

    _add_openings(d)
    _add_columns(d)
    _add_stair(d)

    if not brief.plot_w:
        brief.plot_w = d.W + 2 * side
        brief.plot_d = d.D + front_sb + rear_sb
        d.notes.append("No plot size given: plot sized to the house plus standard setbacks.")
    return d
