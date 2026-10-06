# Builds the scene the editor opens by default: every built-in model on
# display, plus a few physics toys. Regenerate the saved scene with:
#
#   RiftCoreRuntime --headless --scene "" --script Assets/Scripts/build_showcase.py
#
# or just press Run in the editor's Python tab.
import math
import riftcore as rc

rc.new_scene("Showcase")

rc.spawn("plane", name="Ground", scale=(80, 1, 80), color=(0.33, 0.40, 0.33),
         roughness=0.95, physics="static")
rc.spawn("light", name="Sun Light", position=(0, 14, 0), rotation=(38, 35, 0))

shapes = [m for m in rc.models() if m.startswith("primitive:") and m != "primitive:plane"]
props = [m for m in rc.models() if m.startswith("model:")]

palette = [(0.90, 0.30, 0.28), (0.95, 0.60, 0.20), (0.95, 0.85, 0.30), (0.40, 0.75, 0.35),
           (0.25, 0.70, 0.75), (0.25, 0.50, 0.90), (0.55, 0.40, 0.85), (0.85, 0.40, 0.70)]

# Front row: basic shapes on a gentle arc, each in its own colour.
for i, kind in enumerate(shapes):
    t = (i - (len(shapes) - 1) / 2.0)
    x = t * 2.2
    z = 4.0 - 0.06 * t * t
    name = kind.split(":")[1].capitalize()
    rc.spawn(kind, name=name, position=(x, 0.75, z), scale=1.5,
             color=palette[i % len(palette)], roughness=0.35)

# Back rows: props. Buildings and trees are larger.
big = {"model:house": 5.0, "model:tower": 6.0, "model:bridge": 6.0, "model:rocket": 5.0,
       "model:tree_pine": 4.5, "model:tree_round": 4.0, "model:lamp_post": 4.0}
x = -19.0
for i, kind in enumerate(props):
    size = big.get(kind, 2.0)
    z = -5.0 if i % 2 == 0 else -11.0
    name = kind.split(":")[1].replace("_", " ").title()
    rc.spawn(kind, name=name, position=(x, size / 2.0, z), scale=size,
             color=(1, 1, 1), rotation=(0, 20 if i % 3 == 0 else 0, 0))
    x += 2.6

# Physics corner: a crate pyramid and some balls waiting to drop (press Play).
base = 4
for level in range(base):
    for i in range(base - level):
        rc.spawn("model:crate", name="Crate %d-%d" % (level, i),
                 position=(12.0 + (i - (base - level - 1) / 2.0) * 1.05, 0.5 + level * 1.0, 6.0),
                 color=(1, 1, 1), physics="dynamic", mass=2.0, restitution=0.1)

for i in range(5):
    a = i / 5.0 * math.tau
    rc.spawn("sphere", name="Ball %d" % (i + 1),
             position=(12.0 + math.cos(a) * 0.8, 7.0 + i * 1.2, 6.0 + math.sin(a) * 0.8),
             scale=0.8, color=palette[(i * 2) % len(palette)], metallic=0.6, roughness=0.25,
             physics="dynamic", mass=1.0, restitution=0.55)

rc.save_scene("Assets/Scenes/Showcase.json")
print("Showcase scene built:", rc.node_count(), "nodes")
