# Headless engine test, run by CTest through:
#   RiftCoreRuntime --headless --scene "" --script Tests/test_scripting.py --frames 240
# Part 1 runs at load time; part 2 checks the physics result after two
# simulated seconds from an on_update callback.
import os
import tempfile
import riftcore as rc

SCENE_FILE = os.path.join(tempfile.gettempdir(), "riftcore_test_scene.json")

rc.new_scene("ScriptTest")

# --- model library -----------------------------------------------------
models = rc.models()
assert len(models) >= 30, models
for i, kind in enumerate(models):
    rc.spawn(kind, name="m%d" % i, position=(i * 2.0, 50.0, 40.0))
assert rc.node_count() == len(models)
for i in range(len(models)):
    rc.destroy(rc.find("m%d" % i))
assert rc.node_count() == 0

try:
    rc.spawn("no_such_model")
    raise SystemError("unknown model was accepted")
except ValueError:
    pass

# --- transforms and properties ------------------------------------------
ground = rc.spawn("plane", name="Ground", scale=(40, 1, 40), physics="static")
box = rc.spawn("cube", name="Box", position=(0, 5, 0), scale=(1, 2, 1),
               color=(1, 0, 0), physics="dynamic", mass=2.0)
ball = rc.spawn("sphere", name="Ball", position=(3, 3, 0), physics="dynamic",
                restitution=0.0)
car = rc.spawn("model:car", position=(-4, 0.5, 0), scale=3)

assert rc.find("Box") == box and rc.find("missing") is None
info = rc.get(box)
assert info["model"] == "primitive:cube" and info["physics"] == "dynamic"
assert info["collider"] == "box" and abs(info["mass"] - 2.0) < 1e-6
assert rc.get(ball)["collider"] == "sphere"
assert rc.get(ground)["collider"] == "plane"
assert rc.get_scale(car) == (3.0, 3.0, 3.0)

rc.set_position(car, (-4, 1.5, 2))
rc.set_rotation(car, (0, 90, 0))
assert rc.get_position(car) == (-4.0, 1.5, 2.0)
assert rc.get_rotation(car) == (0.0, 90.0, 0.0)
rc.set_color(car, (0.1, 0.2, 0.3))
assert all(abs(a - b) < 1e-6 for a, b in zip(rc.get(car)["color"], (0.1, 0.2, 0.3)))

# parent / child: destroying the parent removes the child
child = rc.spawn("cone", name="Hat", parent=car, position=(0, 1, 0))
assert rc.get(child)["parent"] == car
rc.destroy(car)
assert not rc.exists(child)

# --- save / load round trip ---------------------------------------------
rc.save_scene(SCENE_FILE)
rc.clear()
assert rc.node_count() == 0
rc.load_scene(SCENE_FILE)
assert rc.node_count() == 3
box, ball = rc.find("Box"), rc.find("Ball")
assert rc.get(box)["scale"] == (1.0, 2.0, 1.0)

assert rc.get_gravity()[1] < -9.0
print("part 1 ok:", rc.node_count(), "nodes")

# --- physics: both bodies must come to rest on the ground ------------------
state = {"done": False}

@rc.on_update
def check(dt):
    if state["done"] or rc.time() < 3.0:
        return
    state["done"] = True
    by, sy = rc.get_position(box)[1], rc.get_position(ball)[1]
    print("after %.2fs: box y=%.3f ball y=%.3f" % (rc.time(), by, sy))
    assert 0.8 < by < 1.3, "box should rest on the ground (y~1), got %f" % by
    assert 0.3 < sy < 0.8, "ball should rest on the ground (y~0.5), got %f" % sy
    hit = rc.raycast((0, 20, 0), (0, -1, 0))
    assert hit is not None and hit["distance"] < 20.0
    print("TEST PASSED")
