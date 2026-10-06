"""housegen - automated house design for RiftCore.

    import housegen
    result = housegen.generate("2BHK two storey vastu north facing house on a 9 x 15 m plot")

One prompt (or a Brief) produces a single coordinated building model, and
from it: dimensioned floor plans, four elevations, a section, a foundation /
column layout and a site plan (SVG + one DXF), a design report with room,
door / window and quantity schedules, and the 3D house in the engine scene.

Works inside RiftCore (editor, runtime) and in plain Python, where only the
3D scene is skipped:

    python -m housegen "3 bedroom house with garage" --out Output/MyHouse
"""
import os
import re

from .brief import Brief, parse_prompt
from .model import design_house
from . import drafting, export, vastu

__all__ = ["Brief", "parse_prompt", "design_house", "generate", "set_view"]

OUTPUT_ROOT = "Output"


def _slug(name):
    return re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_") or "House"


def generate(prompt_or_brief, out_dir=None, build_scene=True, new_scene=True):
    """Designs a house and writes every deliverable.

    prompt_or_brief: a text prompt, a Brief, or a dict of Brief fields.
    Returns a dict: design, sheets, out_dir, root (scene node id or 0), summary.
    """
    if isinstance(prompt_or_brief, Brief):
        brief = prompt_or_brief
    elif isinstance(prompt_or_brief, dict):
        brief = Brief.from_dict(prompt_or_brief)
    else:
        brief = parse_prompt(str(prompt_or_brief))

    design = design_house(brief)
    sheets = drafting.all_sheets(design)
    out_dir = out_dir or os.path.join(OUTPUT_ROOT, _slug(brief.name))
    export.write_all(design, sheets, out_dir)

    # The editor's drawing viewer opens whatever was generated last.
    try:
        os.makedirs(OUTPUT_ROOT, exist_ok=True)
        with open(os.path.join(OUTPUT_ROOT, "latest.txt"), "w", encoding="utf-8") as f:
            f.write(os.path.join(out_dir, "design.json").replace("\\", "/"))
    except OSError:
        pass

    root, nodes = 0, 0
    if build_scene:
        try:
            import riftcore
        except ImportError:
            riftcore = None
        if riftcore is not None:
            from . import build3d
            root, nodes = build3d.build(riftcore, design, new_scene)

    summary = "%s: %d x %d mm, %d floor(s), %d rooms, %d sheets -> %s%s" % (
        brief.name, design.W, design.D, design.floors, len(design.rooms), len(sheets), out_dir,
        (", %d scene nodes" % nodes) if nodes else "")
    print(summary)
    if brief.vastu:
        rows = vastu.check(design)
        ok = sum(1 for r in rows if r[2] == "OK")
        print("Vastu check: %d of %d guidelines met" % (ok, len(rows)))
        for rule, result, status in rows:
            if status != "OK":
                print("  note: %s - %s" % (rule, result))
    return {"design": design, "sheets": sheets, "out_dir": out_dir, "root": root, "summary": summary}


def generate_from_file(prompt_file):
    """Editor entry point: the prompt is passed through a text file."""
    with open(prompt_file, "r", encoding="utf-8") as f:
        return generate(f.read())


def set_view(roof=True, upper_floors=True):
    """Shows / hides the roof and the upper floors of the generated house in
    the scene, to look inside the plan in 3D."""
    import riftcore as rc
    for node in rc.nodes():
        name = rc.get(node)["name"]
        if name == "Roof":
            rc.set_active(node, bool(roof))
        elif name in ("First Floor", "Second Floor"):
            rc.set_active(node, bool(upper_floors))
        elif name == "Roof Slab":
            rc.set_active(node, bool(roof))
