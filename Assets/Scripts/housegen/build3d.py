"""Builds the Design as a 3D scene through the riftcore API.

Metres, Y up. The front of the house faces +Z (towards the default camera),
plan X runs along world X and the plan is centred on the origin.
"""
from . import model as M
from . import details3d

WALL_EXT = (0.93, 0.90, 0.84)
WALL_INT = (0.96, 0.95, 0.92)
SLAB = (0.62, 0.62, 0.64)
PLINTH = (0.48, 0.46, 0.44)
ROOF_TILE = (0.62, 0.22, 0.16)
GLASS = (0.45, 0.68, 0.85)
WOOD = (0.45, 0.28, 0.15)
STEP = (0.70, 0.68, 0.64)
LAWN = (0.30, 0.48, 0.26)
PAVING = (0.50, 0.50, 0.52)

# Material (Realistic render mode) for each surface colour.
MATERIALS = {
    WALL_EXT: "paint", WALL_INT: "paint", SLAB: "concrete", PLINTH: "paving",
    ROOF_TILE: "roof_tile", GLASS: "glass", WOOD: "wood", STEP: "concrete",
    LAWN: "grass", PAVING: "paving",
}


class _Builder:
    def __init__(self, rc, design):
        self.rc = rc
        self.d = design
        self.count = 0

    # plan mm -> world metres
    def wx(self, x):
        return (x - self.d.W / 2.0) / 1000.0

    def wz(self, y):
        return (self.d.D / 2.0 - y) / 1000.0

    def box(self, name, parent, x0, x1, y0, y1, z0, z1, color, kind="cube", **kw):
        """Axis-aligned block from plan rectangle [x0,x1] x [y0,y1] and levels [z0,z1] (mm)."""
        if x1 - x0 <= 0 or y1 - y0 <= 0 or z1 - z0 <= 0:
            return 0
        self.count += 1
        material = kw.pop("material", None) or MATERIALS.get(color) or details3d.MATERIALS.get(color)
        if material:
            kw["material"] = material
        return self.rc.spawn(kind, name=name, parent=parent,
                             position=(self.wx((x0 + x1) / 2.0), (z0 + z1) / 2000.0, self.wz((y0 + y1) / 2.0)),
                             scale=((x1 - x0) / 1000.0, (z1 - z0) / 1000.0, (y1 - y0) / 1000.0),
                             color=color, **kw)

    def wall_piece(self, name, parent, w, a, b, z0, z1, color):
        t = w.t / 2.0
        if w.axis == "h":
            return self.box(name, parent, a, b, w.pos - t, w.pos + t, z0, z1, color)
        return self.box(name, parent, w.pos - t, w.pos + t, a, b, z0, z1, color)

    def build(self):
        rc, d = self.rc, self.d
        b = d.brief
        root = rc.spawn("empty", name=b.name)

        # ---- site ----
        site = rc.spawn("empty", name="Site", parent=root)
        side = max(b.setback_side, (b.plot_w - d.W) / 2.0)
        px0, py0 = -side, -b.setback_front
        self.box("Plot", site, px0, px0 + b.plot_w, py0, py0 + b.plot_d, -60, 20, LAWN, roughness=0.95)
        door = next((o for o in d.openings if o.room == "Main Entrance"), None)
        if door:
            self.box("Entrance Path", site, door.start - 200, door.end + 200, py0, -M.EXT_T / 2.0, 20, 60, PAVING)
            self.box("Entrance Step", site, door.start - 300, door.end + 300, -900, -M.EXT_T / 2.0, 60,
                     M.PLINTH - 150, PAVING)
        for o in d.openings:
            if o.kind == "garage":
                self.box("Driveway", site, o.start - 150, o.end + 150, py0, -M.EXT_T / 2.0, 20, 70, PAVING)
        for i, (tx, ty) in enumerate(((px0 + 900, py0 + 900), (px0 + b.plot_w - 900, py0 + 900))):
            rc.spawn("model:tree_round" if i else "model:tree_pine", name="Tree %d" % (i + 1), parent=site,
                     position=(self.wx(tx), 1.75, self.wz(ty)), scale=3.5, color=(1, 1, 1))

        # ---- plinth ----
        e = M.EXT_T / 2.0
        self.box("Plinth", root, -e, d.W + e, -e, d.D + e, 0, M.PLINTH, PLINTH, roughness=0.9)

        # ---- floors ----
        for f in range(d.floors):
            grp = rc.spawn("empty", name=d.floor_name(f), parent=root)
            z = d.ffl(f)
            top = z + d.H - M.SLAB_T
            walls = rc.spawn("empty", name="Walls", parent=grp)
            joinery = rc.spawn("empty", name="Doors and Windows", parent=grp)
            for wi, w in enumerate(w for w in d.walls if w.floor == f):
                color = WALL_EXT if w.exterior else WALL_INT
                ops = sorted((o for o in d.openings_on(f, w.axis, w.pos) if o.start < w.b and o.end > w.a),
                             key=lambda o: o.start)
                ext = e if w.exterior else 0.0
                cur = w.a - ext
                for o in ops:
                    self.wall_piece("Wall", walls, w, cur, o.start, z, top, color)
                    head = min(z + o.sill + o.height, top)
                    if o.sill > 0:
                        self.wall_piece("Sill Wall", walls, w, o.start, o.end, z, z + o.sill, color)
                    self.wall_piece("Lintel Wall", walls, w, o.start, o.end, head, top, color)
                    self.joinery(joinery, w, o, z)
                    cur = o.end
                self.wall_piece("Wall", walls, w, cur, w.b + ext, z, top, color)

            # slab over this floor (the top one is the roof slab), with the stair void left open
            name = "Roof Slab" if f == d.floors - 1 else "Floor Slab"
            st = d.stair
            if st and f < d.floors - 1:
                self.box(name, grp, -e, st.x, -e, d.D + e, top, z + d.H, SLAB)
                self.box(name, grp, st.x + st.w, d.W + e, -e, d.D + e, top, z + d.H, SLAB)
                self.box(name, grp, st.x, st.x + st.w, -e, st.y, top, z + d.H, SLAB)
                self.box(name, grp, st.x, st.x + st.w, st.y + st.d, d.D + e, top, z + d.H, SLAB)
                self.stairs(grp, z)
            else:
                self.box(name, grp, -e, d.W + e, -e, d.D + e, top, z + d.H, SLAB)
            details3d.interior(self, grp, f)

        # ---- roof ----
        roof = rc.spawn("empty", name="Roof", parent=root)
        rl = d.roof_level
        if b.style == "traditional":
            oh = M.ROOF_OVERHANG
            w_m = (d.W + 2 * oh) / 1000.0
            half = (d.D / 2.0 + oh) / 1000.0
            rh = d.ridge_height / 1000.0
            y = rl / 1000.0 + rh / 2.0
            rc.spawn("wedge", name="Roof Front", parent=roof, position=(0, y, half / 2.0),
                     scale=(w_m, rh, half), color=ROOF_TILE, roughness=0.8,
                     material="roof_tile")
            rc.spawn("wedge", name="Roof Rear", parent=roof, position=(0, y, -half / 2.0),
                     rotation=(0, 180, 0), scale=(w_m, rh, half), color=ROOF_TILE, roughness=0.8,
                     material="roof_tile")
            self.count += 2
        else:
            t = M.INT_T
            p0, p1 = rl, rl + M.PARAPET
            self.box("Parapet", roof, -e, d.W + e, -e, -e + t, p0, p1, WALL_EXT)
            self.box("Parapet", roof, -e, d.W + e, d.D + e - t, d.D + e, p0, p1, WALL_EXT)
            self.box("Parapet", roof, -e, -e + t, -e, d.D + e, p0, p1, WALL_EXT)
            self.box("Parapet", roof, d.W + e - t, d.W + e, -e, d.D + e, p0, p1, WALL_EXT)
        details3d.exterior(self, root)
        return root

    def joinery(self, parent, w, o, z):
        """Glass, door leaves and frames inside an opening."""
        if o.kind == "opening":
            return
        zb, zt = z + o.sill, z + o.sill + o.height
        if o.kind == "door":
            details3d.door_leaf(self, parent, w, o, z, w.exterior)
            return
        if o.kind == "window":
            thin = 20
            if w.axis == "h":
                self.box("Window %s" % o.tag, parent, o.start, o.end, w.pos - thin, w.pos + thin, zb, zt,
                         GLASS, metallic=0.7, roughness=0.15)
                self.box("Window Sill", parent, o.start - 60, o.end + 60, w.pos - w.t / 2.0 - 40,
                         w.pos + w.t / 2.0 + 40, zb - 50, zb, SLAB)
                self.box("Mullion", parent, o.start + o.width / 2.0 - 25, o.start + o.width / 2.0 + 25,
                         w.pos - 40, w.pos + 40, zb, zt, WOOD)
            else:
                self.box("Window %s" % o.tag, parent, w.pos - thin, w.pos + thin, o.start, o.end, zb, zt,
                         GLASS, metallic=0.7, roughness=0.15)
                self.box("Window Sill", parent, w.pos - w.t / 2.0 - 40, w.pos + w.t / 2.0 + 40,
                         o.start - 60, o.end + 60, zb - 50, zb, SLAB)
                self.box("Mullion", parent, w.pos - 40, w.pos + 40, o.start + o.width / 2.0 - 25,
                         o.start + o.width / 2.0 + 25, zb, zt, WOOD)
        else:
            color = WOOD if o.kind == "door" else (0.82, 0.82, 0.84)
            thin = 22
            name = "Door %s" % o.tag if o.kind == "door" else "Garage Door"
            if w.axis == "h":
                self.box(name, parent, o.start + 20, o.end - 20, w.pos - thin, w.pos + thin, zb, zt - 20, color)
            else:
                self.box(name, parent, w.pos - thin, w.pos + thin, o.start + 20, o.end - 20, zb, zt - 20, color)

    def stairs(self, parent, z):
        """Dog-leg stair from this floor to the next."""
        st = self.d.stair
        grp = self.rc.spawn("empty", name="Staircase", parent=parent)
        per = st.risers // 2
        k = st.dir
        xl = st.x + 100
        xr = xl + st.flight_w + 100
        rh = st.riser_h
        for i in range(per):                         # first flight (right side), solid steps
            ya = st.y0 + k * i * st.tread
            yb = ya + k * st.tread if i < per - 1 else st.landing_y
            if i == per - 1:
                break
            self.box("Step", grp, xr, xr + st.flight_w, min(ya, yb), max(ya, yb), z, z + rh * (i + 1), STEP)
        mid = z + rh * per
        far = st.y + st.d - 60 if k > 0 else st.y + 60
        land_end = st.landing_y + k * min(1100, abs(far - st.landing_y))
        self.box("Landing", grp, xl, xr + st.flight_w, min(st.landing_y, land_end), max(st.landing_y, land_end),
                 mid - 150, mid, STEP)
        self.box("Landing Support", grp, xr, xr + st.flight_w, min(st.landing_y, land_end),
                 max(st.landing_y, land_end), z, mid - 150, STEP)
        for i in range(per - 1):                     # second flight (left side), back towards the hall
            ya = st.landing_y - k * i * st.tread
            yb = ya - k * st.tread
            level = mid + rh * (i + 1)
            self.box("Step", grp, xl, xl + st.flight_w, min(ya, yb), max(ya, yb), level - 170, level, STEP)


def build(rc, design, new_scene=True):
    """Creates the house in the current scene. Returns (root node id, node count)."""
    if new_scene:
        rc.new_scene(design.brief.name)
        size = max(design.brief.plot_w, design.brief.plot_d) / 1000.0 + 30.0
        rc.spawn("plane", name="Ground", scale=(size, 1, size), color=(0.36, 0.40, 0.34), roughness=0.95,
                 material="grass")
        rc.spawn("light", name="Sun Light", position=(0, 20, 0), rotation=(42, 30, 0))
    builder = _Builder(rc, design)
    root = builder.build()
    return root, builder.count
