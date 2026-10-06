# Per-frame scripting: while the simulation plays, drop a random model every
# quarter second and remove the oldest ones so the scene stays small.
# Run it, then press Play (F5). Run rc.clear_callbacks() to stop the rain.
import random
import riftcore as rc

KINDS = ["cube", "sphere", "cylinder", "cone", "capsule", "gem", "model:crate", "model:barrel"]
MAX_BODIES = 60

state = {"timer": 0.0, "bodies": []}


@rc.on_update
def rain(dt):
    state["timer"] += dt
    if state["timer"] < 0.25:
        return
    state["timer"] = 0.0

    node = rc.spawn(random.choice(KINDS), name="Drop",
                    position=(random.uniform(-6, 6), 14.0, random.uniform(-6, 6)),
                    rotation=(random.uniform(0, 90), random.uniform(0, 360), 0),
                    scale=random.uniform(0.6, 1.4),
                    color=(random.random(), random.random(), random.random()),
                    physics="dynamic", restitution=0.3)
    state["bodies"].append(node)

    while len(state["bodies"]) > MAX_BODIES:
        old = state["bodies"].pop(0)
        if rc.exists(old):
            rc.destroy(old)


print("Rain is armed: press Play. rc.clear_callbacks() stops it.")
