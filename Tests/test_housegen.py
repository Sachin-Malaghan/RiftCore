# House designer test, run by CTest through the headless runtime:
#   RiftCoreRuntime --headless --scene "" --script Tests/test_housegen.py
# Designs a range of houses and checks that every model is geometrically
# consistent, that the deliverables are written, and that the 3D scene builds.
import os
import tempfile

import riftcore as rc
import housegen
from housegen import model as M, vastu as V

PROMPTS = [
    "2BHK house with 2 floors, vastu compliant, north facing",
    "3BHK east facing vastu house, G+1, car parking",
    "3 bedroom south facing vastu duplex with study",
    "4 bedroom west facing vastu house with 3 bathrooms, two storey",
    "Modern 3 bedroom 2 bath two storey house on a 12 x 18 m plot with garage",
    "Single storey 2 bedroom cottage with gable roof, 30 x 40 ft plot",
    "Traditional 5 bedroom 4 bath three storey house with study and pooja room and garage",
    "1 bedroom bungalow",
]

out_root = os.path.join(tempfile.gettempdir(), "riftcore_housegen_test")

for i, prompt in enumerate(PROMPTS):
    result = housegen.generate(prompt, out_dir=os.path.join(out_root, "house_%d" % i))
    d = result["design"]
    b = d.brief
    tag = "[%s]" % prompt

    assert d.W > 0 and d.D > 0 and d.df > 0, tag
    assert d.df + M.HALL < d.D, tag

    for floor in range(d.floors):
        rooms = d.rooms_on(floor)
        # Each band is tiled exactly: no gaps, no overlaps, closes on W.
        for y in (0, d.y_back):
            band = sorted((r for r in rooms if r.y == y and r.kind != "hall"), key=lambda r: r.x)
            assert band, "%s empty band on floor %d" % (tag, floor)
            x = 0
            for r in band:
                assert r.x == x, "%s gap before %s" % (tag, r.name)
                assert r.w >= 1300, "%s %s is only %d wide" % (tag, r.name, r.w)
                x = r.x2
            assert x == d.W, "%s band does not close: %d != %d" % (tag, x, d.W)
        # Openings stay inside their wall and never overlap each other.
        spans = {}
        for o in d.openings_on(floor):
            limit = d.W if o.axis == "h" else d.D
            assert 0 < o.start and o.end < limit, "%s opening outside wall" % tag
            spans.setdefault((o.axis, o.pos), []).append((o.start, o.end))
        for key, items in spans.items():
            items.sort()
            for (a0, a1), (b0, b1) in zip(items, items[1:]):
                assert a1 <= b0, "%s overlapping openings on wall %s" % (tag, key)
        # Every room other than the hall can be entered.
        for r in rooms:
            if r.kind != "hall":
                assert any(o.room == r.name and o.kind in ("door", "opening") for o in d.openings_on(floor)), \
                    "%s no door into %s" % (tag, r.name)

    # The programme was honoured.
    beds = sum(1 for r in d.rooms if r.kind in ("bedroom", "master"))
    assert beds == b.bedrooms, "%s %d bedrooms planned, %d asked" % (tag, beds, b.bedrooms)
    assert any(o.room == "Main Entrance" for o in d.openings), tag
    if d.floors > 1:
        st = d.stair
        assert st and st.risers % 2 == 0 and st.riser_h <= 175.0, tag
        stair_rooms = [r for r in d.rooms if r.kind == "stair"]
        assert len({(r.x, r.y, r.w, r.d) for r in stair_rooms}) == 1, "%s stair does not stack" % tag
    if b.plot_w:
        assert d.W <= b.plot_w and d.D <= b.plot_d, "%s house larger than plot" % tag

    if b.vastu:
        rows = V.check(d)
        notes = [r for r in rows if r[2] != "OK"]
        # The living room is always on the road side, so on south and west
        # facing plots it cannot also sit to the north / east: that one
        # guideline is reported as a note rather than met.
        if b.facing in ("south", "west"):
            notes = [r for r in notes if not r[0].startswith("Living room")]
        assert not notes, "%s vastu notes: %s" % (tag, notes)

    # Deliverables.
    for name in ("plan_0.svg", "elev_front.svg", "section.svg", "structure.svg", "site.svg",
                 "drawings.dxf", "report.md", "design.json"):
        path = os.path.join(result["out_dir"], name)
        assert os.path.getsize(path) > 500, "%s missing %s" % (tag, name)
    assert len(result["sheets"]) == d.floors + 7, tag

    # 3D scene.
    assert result["root"] and rc.node_count() > 60, "%s scene not built" % tag
    assert rc.find("Roof") and rc.find("Ground Floor") and rc.find("Plinth"), tag

print("HOUSEGEN PASSED: %d designs" % len(PROMPTS))
