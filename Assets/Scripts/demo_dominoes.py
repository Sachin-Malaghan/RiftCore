# A curved line of dominoes and a ball to knock the first one over.
# Run it, then press Play (F5).
import math
import riftcore as rc

COUNT = 40
RADIUS = 9.0

for i in range(COUNT):
    a = i / COUNT * math.pi * 1.5
    x, z = math.cos(a) * RADIUS, math.sin(a) * RADIUS
    rc.spawn("cube", name="Domino %02d" % i,
             position=(x, 1.0, z),
             rotation=(0, math.degrees(a), 0),
             scale=(1.0, 2.0, 0.25),
             color=(0.9 - i / COUNT * 0.6, 0.3 + i / COUNT * 0.4, 0.9),
             physics="dynamic", mass=1.0, restitution=0.05, friction=0.8)

ball = rc.spawn("sphere", name="Striker", position=(RADIUS, 1.2, -3.0), scale=1.2,
                color=(1.0, 0.8, 0.2), metallic=0.8, roughness=0.2,
                physics="dynamic", mass=6.0)
rc.set_velocity(ball, (0, 0, 9))
print("Placed", COUNT, "dominoes. Press Play to start the chain reaction.")
