"""Vastu Shastra support: compass-aware room zoning and a compliance check.

The plan's front wall (y = 0) faces the road, so the compass depends on the
plot's facing. Zoning follows the commonly cited residential guidelines:

    entrance    north / east facing: north-east half of the front
                south facing: south-east half, west facing: north-west half
    pooja       north-east            kitchen   south-east (fire corner)
    master bed  south-west            toilets   north-west (never north-east)
    staircase   south-west / south / west (never north-east)
    living      north / east side

These are traditional planning preferences, not building regulations.
"""

FACINGS = ("north", "east", "south", "west")
OPPOSITE = {"north": "south", "south": "north", "east": "west", "west": "east"}

# Compass direction that plan +X points to, for each facing (+Y is the rear).
X_DIR = {"north": "west", "east": "north", "south": "east", "west": "south"}

# facing -> compass corner -> (band, end)   end 0 = x at 0, end 1 = x at W
CORNERS = {
    "north": {"NE": ("front", 0), "NW": ("front", 1), "SE": ("back", 0), "SW": ("back", 1)},
    "east":  {"SE": ("front", 0), "NE": ("front", 1), "SW": ("back", 0), "NW": ("back", 1)},
    "south": {"SW": ("front", 0), "SE": ("front", 1), "NW": ("back", 0), "NE": ("back", 1)},
    "west":  {"NW": ("front", 0), "SW": ("front", 1), "NE": ("back", 0), "SE": ("back", 1)},
}

# Favourable end of the front wall for the main door.
ENTRANCE_CORNER = {"north": "NE", "east": "NE", "south": "SE", "west": "NW"}

_VEC = {"north": (0, 1), "south": (0, -1), "east": (1, 0), "west": (-1, 0)}   # (east, north)


def entrance_prefer(facing):
    """'start' or 'end' of the front wall (low or high X) for the main door."""
    band, end = CORNERS[facing][ENTRANCE_CORNER[facing]]
    return "start" if end == 0 else "end"


def north_vector(facing):
    """Direction of north in plan coordinates (x, y)."""
    xe, xn = _VEC[X_DIR[facing]]
    ye, yn = _VEC[OPPOSITE[facing]]
    return (xn, yn)


def face_compass(facing):
    """Compass direction each elevation looks out to."""
    return {"front": facing, "rear": OPPOSITE[facing],
            "right": X_DIR[facing], "left": OPPOSITE[X_DIR[facing]]}


def zone_of(x, y, W, D, facing):
    """Compass zone ('NE', 'S', 'C', ...) of a plan point."""
    px = (x - W / 2.0) / (W / 2.0)
    py = (y - D / 2.0) / (D / 2.0)
    xe, xn = _VEC[X_DIR[facing]]
    ye, yn = _VEC[OPPOSITE[facing]]
    east = px * xe + py * ye
    north = px * xn + py * yn
    ns = "N" if north > 0.25 else "S" if north < -0.25 else ""
    ew = "E" if east > 0.25 else "W" if east < -0.25 else ""
    return (ns + ew) or "C"


class Bands:
    """Front and rear room lists built from compass corners inward."""

    def __init__(self, facing):
        self.facing = facing
        self.ends = {("front", 0): [], ("front", 1): [], ("back", 0): [], ("back", 1): []}
        self.mid = {"front": [], "back": []}

    def corner(self, compass, item):
        self.ends[CORNERS[self.facing][compass]].append(item)
        return CORNERS[self.facing][compass][0]

    def band_of(self, compass):
        return CORNERS[self.facing][compass][0]

    def middle(self, band, item):
        self.mid[band].append(item)

    def front_outer(self, item, prefer=("NW", "SE", "NE", "SW")):
        """Puts a room (garage) at an outer end of the front band."""
        for compass in prefer:
            band, end = CORNERS[self.facing][compass]
            if band == "front":
                self.ends[(band, end)].insert(0, item)
                return

    def items(self, band):
        return self.ends[(band, 0)] + self.mid[band] + list(reversed(self.ends[(band, 1)]))


def check(design):
    """Returns [(rule, result, 'OK' | 'NOTE')] for the finished design."""
    b = design.brief
    f = b.facing
    W, D = design.W, design.D
    rows = []

    def zone(r):
        return zone_of(r.x + r.w / 2.0, r.y + r.d / 2.0, W, D, f)

    def rule(label, rooms, good, bad=()):
        for r in rooms:
            z = zone(r)
            ok = z in good and z not in bad
            rows.append(("%s (%s, %s)" % (label, r.name, design.floor_name(r.floor)),
                         "in the %s zone" % z, "OK" if ok else "NOTE"))

    door = next((o for o in design.openings if o.room == "Main Entrance"), None)
    if door:
        z = zone_of(door.start + door.width / 2.0, 0, W, D, f)
        want = ENTRANCE_CORNER[f]
        ok = want[0] in z and want[1] in z or z == want
        rows.append(("Main entrance on the %s face, %s side" % (f, want),
                     "door centre in the %s zone" % z, "OK" if ok else "NOTE"))

    rooms = design.rooms
    rule("Pooja in the north-east", [r for r in rooms if r.kind == "pooja"], ("NE", "N", "E"))
    rule("Kitchen in the south-east", [r for r in rooms if r.kind == "kitchen"], ("SE", "E", "S", "NW"))
    rule("Master bedroom in the south-west", [r for r in rooms if r.kind == "master"], ("SW", "S", "W"))
    rule("Staircase in the south / west", [r for r in rooms if r.kind == "stair"],
         ("SW", "S", "W", "NW", "SE"), ("NE",))
    rule("Toilet away from the north-east", [r for r in rooms if r.kind == "bath"],
         ("NW", "W", "S", "N", "SE", "E", "C"), ("NE", "SW"))
    rule("Living room to the north / east", [r for r in rooms if r.kind == "living"],
         ("N", "NE", "E", "NW", "C", "SE"))
    return rows
