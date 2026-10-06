"""Design brief: what the client asked for.

parse_prompt() turns free text such as

    "modern 3 bedroom 2 bath two storey house on a 12 x 18 m plot with garage"

into a Brief. Anything the prompt does not mention keeps a sensible default,
and a Brief can also be filled in directly (or from JSON) by a script.
All lengths are millimetres.
"""
import re
from dataclasses import dataclass, field, asdict

FT = 304.8

_WORDS = {"one": 1, "single": 1, "two": 2, "double": 2, "three": 3, "triple": 3,
          "four": 4, "five": 5, "six": 6, "a": 1}


@dataclass
class Brief:
    name: str = "House"
    prompt: str = ""
    floors: int = 1
    bedrooms: int = 2
    bathrooms: int = 2
    garage: bool = False
    study: bool = False
    pooja: bool = False
    facing: str = "north"          # compass direction the front (road side) faces
    vastu: bool = False            # zone the rooms by Vastu Shastra guidelines
    style: str = "modern"          # "modern" (flat roof + parapet) or "traditional" (gable roof)
    plot_w: int = 0                # 0 = not given: the plot is sized around the house
    plot_d: int = 0
    target_area_m2: float = 0.0    # built-up area per floor, 0 = from the room program
    floor_height: int = 3000       # floor to floor
    setback_front: int = 3000
    setback_side: int = 1500
    setback_rear: int = 1500
    notes: list = field(default_factory=list)

    def to_dict(self):
        return asdict(self)

    @staticmethod
    def from_dict(d):
        b = Brief()
        for k, v in d.items():
            if hasattr(b, k):
                setattr(b, k, v)
        return b


def _num(token):
    token = token.lower()
    if token in _WORDS:
        return _WORDS[token]
    try:
        return float(token)
    except ValueError:
        return None


def _count(text, pattern, default):
    """Number written before a keyword: '3 bed', 'three bedrooms', '3bhk'."""
    m = re.search(r"(\d+|one|two|three|four|five|six|single|double|a)\s*[- ]?\s*(?:%s)" % pattern, text)
    if not m:
        return default
    n = _num(m.group(1))
    return int(n) if n else default


def _to_mm(value, unit):
    unit = (unit or "m").strip().lower()
    if unit in ("ft", "feet", "foot", "'"):
        return value * FT
    if unit == "mm":
        return value
    return value * 1000.0


def parse_prompt(prompt):
    """Rule-based reading of a design prompt. Returns a Brief."""
    b = Brief(prompt=prompt.strip())
    t = " " + prompt.lower().replace(",", " ") + " "

    # Bedrooms / bathrooms ("3 bhk" implies bedrooms; baths default to beds - 1, min 1).
    beds = _count(t, r"bhk|bed\s*rooms?|beds?\b|br\b", 0)
    if beds:
        b.bedrooms = max(1, min(beds, 8))
    baths = _count(t, r"bath\s*rooms?|baths?\b|toilets?|washrooms?", 0)
    b.bathrooms = max(1, min(baths, 8)) if baths else max(1, b.bedrooms - 1)

    # Storeys.
    floors = _count(t, r"stor(?:e)?y|stor(?:e)?ys|stories|storeys|floors?\b|levels?\b", 0)
    m = re.search(r"\bg\s*\+\s*(\d)", t)
    if m:
        floors = int(m.group(1)) + 1
    if not floors and re.search(r"duplex|double[- ]stor", t):
        floors = 2
    if not floors and re.search(r"bungalow|single[- ]stor|ground floor only", t):
        floors = 1
    if floors:
        b.floors = max(1, min(floors, 3))
    elif b.bedrooms >= 4:
        b.floors = 2
        b.notes.append("Storeys not stated: 2 chosen for %d bedrooms." % b.bedrooms)

    # Plot: "12 x 18 m", "40x60 ft", "30 by 40 feet".
    m = re.search(r"(\d+(?:\.\d+)?)\s*(m|ft|feet|foot|')?\s*(?:x|by|\*)\s*(\d+(?:\.\d+)?)\s*"
                  r"(mm|m\b|meters?|metres?|ft|feet|foot|')?", t)
    if m:
        unit = m.group(4) or m.group(2) or "m"
        if unit.startswith("met"):
            unit = "m"
        w = _to_mm(float(m.group(1)), unit)
        d = _to_mm(float(m.group(3)), unit)
        if 4000 <= w <= 100000 and 4000 <= d <= 100000:
            b.plot_w, b.plot_d = int(round(w)), int(round(d))

    # Built-up area: "1500 sq ft", "140 sqm", "140 m2".
    m = re.search(r"(\d+(?:\.\d+)?)\s*(sq\.?\s*ft|sqft|square\s*feet|sft|sq\.?\s*m|sqm|m2|square\s*met)", t)
    if m:
        area = float(m.group(1))
        if "f" in m.group(2):
            area *= 0.092903
        b.target_area_m2 = area

    m = re.search(r"(north|south|east|west)\s*[- ]?\s*facing|facing\s+(north|south|east|west)", t)
    if m:
        b.facing = m.group(1) or m.group(2)
    b.vastu = bool(re.search(r"vastu|vaastu|vasthu", t))
    if b.vastu:
        b.pooja = True

    b.garage = bool(re.search(r"garage|car\s*park|carport|parking", t))
    b.study = bool(re.search(r"study|office|library|work\s*room", t))
    b.pooja = b.pooja or bool(re.search(r"pooja|puja|prayer|mandir", t))

    if re.search(r"traditional|gable|pitched|sloped|sloping|tiled roof|cottage|villa|farmhouse", t):
        b.style = "traditional"
    if re.search(r"modern|contemporary|flat roof|minimal", t):
        b.style = "modern"

    m = re.search(r"(\d+(?:\.\d+)?)\s*(m|ft|feet)\s*(?:floor|ceiling)\s*height", t)
    if m:
        h = _to_mm(float(m.group(1)), m.group(2))
        if 2600 <= h <= 4500:
            b.floor_height = int(round(h / 50.0) * 50)

    b.name = "%d Bedroom %s House%s" % (b.bedrooms, b.style.capitalize(),
                                       " (Vastu, %s facing)" % b.facing if b.vastu else "")
    return b
