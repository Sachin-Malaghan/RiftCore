#!/usr/bin/env python3
"""Downloads real PBR texture sets for the Realistic render mode.

Source: Poly Haven (https://polyhaven.com) - every asset is CC0 (public
domain), free for any use, no attribution required.

For each engine material this fetches the albedo, OpenGL normal and roughness
maps and saves them as

    Assets/Textures/<material>/albedo.jpg   normal.jpg   rough.jpg

which is exactly where the engine looks before falling back to its built-in
generated textures. Nothing else is touched.

    python Tools/fetch_assets.py --dry-run     list what would be downloaded, with sizes
    python Tools/fetch_assets.py               download at 1k (about 1-3 MB per material)
    python Tools/fetch_assets.py --res 2k      sharper, about 4x larger
    python Tools/fetch_assets.py --only brick,wood_floor
"""
import argparse
import json
import os
import sys
import urllib.error
import urllib.request

API = "https://api.polyhaven.com"
HEADERS = {"User-Agent": "RiftCore-fetch-assets/1.0"}

# engine material -> Poly Haven texture ids to try, in order of preference.
# The first id that exists is used; the choice is recorded in CREDITS.md.
CANDIDATES = {
    "paint":      ["white_plaster_02", "painted_plaster_wall", "plastered_wall", "beige_wall_001"],
    "concrete":   ["concrete_floor_02", "concrete_wall_008", "concrete_floor_worn_001"],
    "wood_floor": ["wood_floor_deck", "laminate_floor_02", "wood_floor", "herringbone_parquet"],
    "wood":       ["wood_table_001", "oak_veneer_01", "wood_cabinet_worn_long", "plywood"],
    "wood_dark":  ["dark_wood", "wood_table_worn", "dark_wooden_planks", "walnut_veneer"],
    "tile":       ["floor_tiles_06", "tiled_floor_001", "square_tiles", "floor_tiles_08"],
    "marble":     ["marble_01", "marble_tiles", "white_marble_tiles"],
    "bath_tile":  ["blue_floor_tiles_01", "bathroom_tiles", "mosaic_tiles", "tiles_0015"],
    "brick":      ["red_brick_03", "brick_wall_001", "red_brick", "red_bricks_04"],
    "roof_tile":  ["roof_tiles_14", "clay_roof_tiles_02", "roof_tiles", "ceramic_roof_01"],
    "grass":      ["aerial_grass_rock", "leafy_grass", "forrest_ground_01", "grass_path_2"],
    "paving":     ["cobblestone_floor_08", "concrete_pavers", "paving_stones", "pavement_02"],
    "granite":    ["granite_tile", "rock_tile_floor", "black_granite"],
    "fabric":     ["fabric_pattern_07", "cotton_jersey", "denim_fabric", "fabric_pattern_05"],
}

MAPS = (("albedo", ("Diffuse", "diff", "Color")),
        ("normal", ("nor_gl", "Normal")),
        ("rough",  ("Rough", "rough", "Roughness")))


def get_json(url):
    req = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode("utf-8"))


def pick(files, keys, res):
    """(url, size) of a map at the wanted resolution, preferring JPG."""
    for key in keys:
        node = files.get(key)
        if not node:
            continue
        by_res = node.get(res) or node.get("1k") or next(iter(node.values()), None)
        if not by_res:
            continue
        for fmt in ("jpg", "png"):
            if fmt in by_res:
                return by_res[fmt]["url"], by_res[fmt].get("size", 0), fmt
    return None


def resolve(material, res):
    """First candidate that exists: (asset id, {map: (url, size, fmt)})."""
    for asset in CANDIDATES[material]:
        try:
            files = get_json("%s/files/%s" % (API, asset))
        except (urllib.error.HTTPError, urllib.error.URLError, ValueError):
            continue
        maps = {}
        for name, keys in MAPS:
            found = pick(files, keys, res)
            if found:
                maps[name] = found
        if "albedo" in maps:
            return asset, maps
    return None, {}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--res", default="1k", choices=["1k", "2k", "4k"])
    ap.add_argument("--only", default="", help="comma-separated material names")
    ap.add_argument("--dry-run", action="store_true", help="list files and sizes, download nothing")
    ap.add_argument("--force", action="store_true", help="replace textures that are already there")
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_root = os.path.join(root, "Assets", "Textures")
    wanted = [m.strip() for m in args.only.split(",") if m.strip()] or list(CANDIDATES)

    total, credits, missing = 0, [], []
    for material in wanted:
        if material not in CANDIDATES:
            print("unknown material: %s" % material)
            continue
        out_dir = os.path.join(out_root, material)
        if os.path.isdir(out_dir) and os.listdir(out_dir) and not args.force:
            print("%-11s already present (use --force to replace)" % material)
            continue
        asset, maps = resolve(material, args.res)
        if not asset:
            missing.append(material)
            print("%-11s no matching Poly Haven texture found - the built-in texture stays in use" % material)
            continue
        size = sum(m[1] for m in maps.values())
        total += size
        print("%-11s <- polyhaven.com/a/%s  (%s, %.1f MB)" % (material, asset, ", ".join(sorted(maps)), size / 1e6))
        credits.append((material, asset))
        if args.dry_run:
            continue
        os.makedirs(out_dir, exist_ok=True)
        for name, (url, _, fmt) in maps.items():
            req = urllib.request.Request(url, headers=HEADERS)
            with urllib.request.urlopen(req, timeout=120) as r, \
                    open(os.path.join(out_dir, "%s.%s" % (name, fmt)), "wb") as f:
                f.write(r.read())

    print("\n%s %.1f MB in %d material(s)." % ("Would download" if args.dry_run else "Downloaded",
                                                total / 1e6, len(credits)))
    if missing:
        print("Not found: %s" % ", ".join(missing))
    if credits and not args.dry_run:
        with open(os.path.join(out_root, "CREDITS.md"), "w", encoding="utf-8") as f:
            f.write("# Texture credits\n\nAll textures are from Poly Haven (https://polyhaven.com), "
                    "licensed CC0 (public domain).\n\n| Material | Poly Haven asset |\n|---|---|\n")
            for material, asset in credits:
                f.write("| %s | https://polyhaven.com/a/%s |\n" % (material, asset))
    return 0


if __name__ == "__main__":
    sys.exit(main())
