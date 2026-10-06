# RiftCore Python scripting

Scripts are ordinary Python files that import the built-in `riftcore`
module. Run them from the editor's **Python** tab, type one-liners into the
**Console** prompt, or run them without the editor:

```
RiftCoreRuntime --script Assets/Scripts/demo_dominoes.py
RiftCoreRuntime --headless --frames 600 --script my_simulation.py
```

`riftcore.api()` prints the full function list at any time.

## Building a scene

```python
import riftcore as rc

ground = rc.spawn("plane", name="Ground", scale=(40, 1, 40), physics="static")
box = rc.spawn("cube", position=(0, 5, 0), color=(1, 0.2, 0.2), physics="dynamic", mass=2)
car = rc.spawn("model:car", position=(4, 1, 0), scale=2)
```

`spawn(kind, name=None, position=(0,0,0), rotation=(0,0,0), scale=1,
color=(.8,.8,.8), physics=None, mass=1, metallic=0, roughness=.5,
restitution=.4, friction=.5, parent=0)` returns the node id.

* `kind` is a name from `rc.models()` (`"cube"`, `"sphere"`, `"model:tree_pine"`,
  ...), a path to an `.obj` file, `"empty"` or `"light"`.
* `scale` is one number or `(x, y, z)`. Built-in models fit a 1 m cube, so
  the scale is the size in metres.
* `physics` is `None`, `"static"` or `"dynamic"`; the collider is sized to
  the model automatically. Rotations are in degrees.

## Functions

| Scene | |
|---|---|
| `spawn(...)`, `destroy(id)`, `clear()` | create / remove nodes |
| `find(name)`, `nodes()`, `exists(id)`, `node_count()` | look nodes up |
| `get(id)` | dict with every property of a node |
| `set_name`, `set_active` | rename, show / hide |
| `get_position` / `set_position`, `get_rotation` / `set_rotation`, `get_scale` / `set_scale` | transform |
| `set_color(id, (r,g,b))`, `set_material(id, metallic=, roughness=)`, `set_model(id, kind)` | appearance |
| `new_scene(name)`, `save_scene(path)`, `load_scene(path)` | scene files |
| `models()` | names of the built-in models |

| Physics | |
|---|---|
| `set_physics(id, mode, mass=, restitution=, friction=)` | `mode`: `None`, `"static"`, `"dynamic"` |
| `get_velocity` / `set_velocity`, `apply_impulse`, `apply_force` | drive bodies |
| `get_gravity()` / `set_gravity((x,y,z))` | world gravity |
| `raycast(origin, direction, max_distance=1000)` | dict (`distance`, `point`, `normal`) or `None` |

| Simulation | |
|---|---|
| `on_update(fn)` | call `fn(dt)` every simulated frame; also works as a decorator |
| `clear_callbacks()` | remove every `on_update` function |
| `time()`, `is_simulating()` | simulated seconds, play state |
| `log(text)` | write to the engine log and the console |

In the editor, `on_update` functions run only while the simulation is
playing, and pressing Stop restores the scene to how it was before Play.

## Samples

* `housegen/` - automated house design from a prompt (see its README).

* `build_showcase.py` - builds the default scene with every model.
* `demo_dominoes.py` - a domino chain reaction.
* `demo_rain.py` - spawns and recycles falling bodies from a per-frame callback.
* `demo_village.py` - procedural level layout.
