# Procedural level building: a small village laid out on a grid, with a
# ring of trees, street lamps along the road and a bridge.
import math
import random
import riftcore as rc

random.seed(7)

# Houses along both sides of a road running down the X axis.
for i in range(6):
    x = -15 + i * 6
    for side in (-1, 1):
        rc.spawn("model:house", name="House",
                 position=(x, 2.5, side * 7.0), scale=5.0,
                 rotation=(0, 0 if side < 0 else 180, 0), physics="static")
    rc.spawn("model:lamp_post", name="Lamp", position=(x + 3, 2.0, 2.6), scale=4.0)

# The road itself.
rc.spawn("cube", name="Road", position=(0, 0.03, 0), scale=(44, 0.06, 4),
         color=(0.22, 0.22, 0.24), roughness=0.9)

# A forest ring around the village.
for i in range(48):
    a = i / 48.0 * math.tau
    r = 26.0 + random.uniform(-2.5, 2.5)
    kind = "model:tree_pine" if i % 3 else "model:tree_round"
    size = random.uniform(3.5, 6.0)
    rc.spawn(kind, name="Tree", position=(math.cos(a) * r, size / 2.0, math.sin(a) * r), scale=size)

# Landmarks.
rc.spawn("model:tower", name="Watch Tower", position=(-22, 4.0, 0), scale=8.0, physics="static")
rc.spawn("model:bridge", name="Bridge", position=(22, 1.5, 0), scale=(8, 3, 6), rotation=(0, 0, 0))
rc.spawn("model:car", name="Car", position=(-4, 0.75, 0.9), scale=2.2, physics="dynamic", mass=8.0)
rc.spawn("model:rocket", name="Rocket", position=(0, 3.0, -16), scale=6.0)

print("Village built:", rc.node_count(), "nodes")
