"""Unit systems for everything the designer writes out.

The building model is always held in millimetres; a unit system only
decides how lengths, levels, areas and quantities are *written* on the
drawings, in the report and in the editor.

    "mm"     3600            metric, millimetres (default; usual on working drawings)
    "cm"     360.0           metric, centimetres
    "m"      3.600           metric, metres
    "in"     141 3/4"        imperial, inches to the nearest 1/8"
    "ft-in"  11'-9 3/4"      imperial, feet and inches to the nearest 1/8"
"""

SYSTEMS = ("mm", "cm", "m", "in", "ft-in")

NAMES = {
    "mm": "millimetres", "cm": "centimetres", "m": "metres",
    "in": "inches", "ft-in": "feet and inches",
}

MM_PER_INCH = 25.4
SQFT_PER_SQM = 10.7639104
CUFT_PER_CUM = 35.3146667
LB_PER_KG = 2.20462262

_current = "mm"


def set_units(units):
    """Selects the unit system used by the fmt_* functions."""
    global _current
    _current = normalise(units)
    return _current


def get_units():
    return _current


def normalise(units):
    u = (units or "mm").strip().lower().replace(" ", "")
    aliases = {
        "millimetre": "mm", "millimeter": "mm", "millimetres": "mm", "millimeters": "mm",
        "centimetre": "cm", "centimeter": "cm", "centimetres": "cm", "centimeters": "cm",
        "metre": "m", "meter": "m", "metres": "m", "meters": "m", "metric": "mm",
        "inch": "in", "inches": "in", "\"": "in",
        "ft": "ft-in", "feet": "ft-in", "foot": "ft-in", "ftin": "ft-in", "feet-inches": "ft-in",
        "imperial": "ft-in", "'": "ft-in",
    }
    u = aliases.get(u, u)
    return u if u in SYSTEMS else "mm"


def is_imperial(units=None):
    return (units or _current) in ("in", "ft-in")


def _inches(mm):
    """(whole inches, eighths) rounded to the nearest 1/8 inch."""
    eighths = int(round(abs(mm) / MM_PER_INCH * 8.0))
    return eighths // 8, eighths % 8


def _frac(eighths):
    """'3/4', '1/8' ... for a count of eighths (1..7)."""
    n, d = eighths, 8
    while n % 2 == 0 and d > 1:
        n //= 2
        d //= 2
    return "%d/%d" % (n, d)


def fmt_len(mm, units=None):
    """A length as it is written on a drawing."""
    u = units or _current
    if u == "mm":
        return "%d" % round(mm)
    if u == "cm":
        return "%.1f" % (mm / 10.0)
    if u == "m":
        return "%.3f" % (mm / 1000.0)
    whole, eighths = _inches(mm)
    sign = "-" if mm < 0 and (whole or eighths) else ""
    frac = (" " + _frac(eighths)) if eighths else ""
    if u == "in":
        return '%s%d%s"' % (sign, whole, frac)
    feet, inch = whole // 12, whole % 12
    return "%s%d'-%d%s\"" % (sign, feet, inch, frac)


def fmt_size(w_mm, d_mm, units=None):
    return "%s x %s" % (fmt_len(w_mm, units), fmt_len(d_mm, units))


def fmt_level(mm, units=None):
    """A level relative to ground, always signed."""
    u = units or _current
    if u in ("mm", "cm", "m"):
        return "%+.3f" % (mm / 1000.0)        # levels are conventionally in metres
    text = fmt_len(abs(mm), u)
    return ("-" if mm < 0 else "+") + text


def fmt_area(sqm, units=None):
    if is_imperial(units):
        return "%.1f sq.ft" % (sqm * SQFT_PER_SQM)
    return "%.2f sq.m" % sqm


def fmt_quantity(value, unit, units=None):
    """Converts a quantity (value, 'sq.m' | 'cu.m' | 'kg' | 'nos') for the report."""
    if not is_imperial(units) or unit == "nos":
        return value, unit
    if unit == "sq.m":
        return value * SQFT_PER_SQM, "sq.ft"
    if unit == "cu.m":
        return value * CUFT_PER_CUM, "cu.ft"
    if unit == "kg":
        return value * LB_PER_KG, "lb"
    return value, unit


def note(units=None):
    u = units or _current
    if u == "ft-in":
        return "all dimensions in feet and inches (to 1/8\")"
    if u == "in":
        return "all dimensions in inches (to 1/8\")"
    return "all dimensions in %s" % NAMES[u]
