"""Command line:  python -m housegen "<prompt>" [--out folder]

Run from Assets/Scripts (or with it on PYTHONPATH). Produces the drawings
and the report; the 3D scene needs the engine (use the editor or
RiftCoreRuntime --script).
"""
import sys

from . import generate


def main(argv):
    out = None
    if "--out" in argv:
        i = argv.index("--out")
        out = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    prompt = " ".join(argv).strip()
    if not prompt:
        print(__doc__)
        return 1
    generate(prompt, out_dir=out, build_scene=False)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
