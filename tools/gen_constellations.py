#!/usr/bin/env python3
"""Build src/c/constellation_data.h from IAU stick-figure polylines."""
from __future__ import print_function

import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools", "constellations.lines.json")
OUT = os.path.join(ROOT, "src", "c", "constellation_data.h")

IAU_NAMES = {
    "And": "Andromeda",
    "Ant": "Antlia",
    "Aps": "Apus",
    "Aql": "Aquila",
    "Aqr": "Aquarius",
    "Ara": "Ara",
    "Ari": "Aries",
    "Aur": "Auriga",
    "Boo": "Bootes",
    "Cae": "Caelum",
    "Cam": "Camelopardalis",
    "Cap": "Capricornus",
    "Car": "Carina",
    "Cas": "Cassiopeia",
    "Cen": "Centaurus",
    "Cep": "Cepheus",
    "Cet": "Cetus",
    "Cha": "Chamaeleon",
    "Cir": "Circinus",
    "CMa": "Canis Major",
    "CMi": "Canis Minor",
    "Cnc": "Cancer",
    "Col": "Columba",
    "Com": "Coma Berenices",
    "CrA": "Corona Australis",
    "CrB": "Corona Borealis",
    "Crt": "Crater",
    "Cru": "Crux",
    "Crv": "Corvus",
    "CVn": "Canes Venatici",
    "Cyg": "Cygnus",
    "Del": "Delphinus",
    "Dor": "Dorado",
    "Dra": "Draco",
    "Equ": "Equuleus",
    "Eri": "Eridanus",
    "For": "Fornax",
    "Gem": "Gemini",
    "Gru": "Grus",
    "Her": "Hercules",
    "Hor": "Horologium",
    "Hya": "Hydra",
    "Hyi": "Hydrus",
    "Ind": "Indus",
    "Lac": "Lacerta",
    "Leo": "Leo",
    "Lep": "Lepus",
    "Lib": "Libra",
    "LMi": "Leo Minor",
    "Lup": "Lupus",
    "Lyn": "Lynx",
    "Lyr": "Lyra",
    "Men": "Mensa",
    "Mic": "Microscopium",
    "Mon": "Monoceros",
    "Mus": "Musca",
    "Nor": "Norma",
    "Oct": "Octans",
    "Oph": "Ophiuchus",
    "Ori": "Orion",
    "Pav": "Pavo",
    "Peg": "Pegasus",
    "Per": "Perseus",
    "Phe": "Phoenix",
    "Pic": "Pictor",
    "PsA": "Piscis Austrinus",
    "Psc": "Pisces",
    "Pup": "Puppis",
    "Pyx": "Pyxis",
    "Ret": "Reticulum",
    "Scl": "Sculptor",
    "Sco": "Scorpius",
    "Sct": "Scutum",
    "Ser": "Serpens",
    "Sex": "Sextans",
    "Sge": "Sagitta",
    "Sgr": "Sagittarius",
    "Tau": "Taurus",
    "Tel": "Telescopium",
    "TrA": "Triangulum Australe",
    "Tri": "Triangulum",
    "Tuc": "Tucana",
    "UMa": "Ursa Major",
    "UMi": "Ursa Minor",
    "Vel": "Vela",
    "Vir": "Virgo",
    "Vol": "Volans",
    "Vul": "Vulpecula",
}


def wrap_ra(ra):
    while ra < 0.0:
        ra += 360.0
    while ra >= 360.0:
        ra -= 360.0
    return ra


def pack_ra(ra):
    ra = wrap_ra(ra)
    v = int(round((ra / 360.0) * 65536.0)) & 0xFFFF
    return 0 if v == 65536 else v


def pack_dec(dec):
    c = int(round(dec * 100.0))
    if c > 32767:
        return 32767
    if c < -32768:
        return -32768
    return c


def unit_from_ra_dec(ra, dec):
    ra_r = math.radians(wrap_ra(ra))
    dec_r = math.radians(dec)
    c = math.cos(dec_r)
    return (c * math.cos(ra_r), c * math.sin(ra_r), math.sin(dec_r))


def ra_dec_from_unit(v):
    x, y, z = v
    n = math.sqrt(x * x + y * y + z * z) or 1.0
    x, y, z = x / n, y / n, z / n
    dec = math.degrees(math.asin(max(-1.0, min(1.0, z))))
    ra = math.degrees(math.atan2(y, x))
    return wrap_ra(ra), dec


def main():
    with open(SRC, "r", encoding="utf-8") as f:
        data = json.load(f)

    grouped = {}
    for feat in data["features"]:
        abb = feat["id"]
        name = IAU_NAMES.get(abb)
        if not name:
            print("unknown abbreviation", abb, file=sys.stderr)
            continue
        segs = grouped.setdefault(name, [])
        for line in feat["geometry"]["coordinates"]:
            pts = [(float(p[0]), float(p[1])) for p in line]
            for i in range(len(pts) - 1):
                segs.append((pts[i], pts[i + 1]))

    constellations = []
    vertices = []
    vert_index = {}
    all_segs = []

    def vid(pt):
        ra, dec = wrap_ra(pt[0]), pt[1]
        key = (round(ra, 3), round(dec, 3))
        if key in vert_index:
            return vert_index[key]
        idx = len(vertices)
        vertices.append((ra, dec))
        vert_index[key] = idx
        return idx

    for name in sorted(grouped):
        segs = grouped[name]
        first = len(all_segs)
        xs = ys = zs = 0.0
        seen = set()
        n_ok = 0
        for a, b in segs:
            ia, ib = vid(a), vid(b)
            if ia == ib:
                continue
            all_segs.append((ia, ib))
            for pt in (a, b):
                key = (round(wrap_ra(pt[0]), 3), round(pt[1], 3))
                if key in seen:
                    continue
                seen.add(key)
                ux, uy, uz = unit_from_ra_dec(pt[0], pt[1])
                xs += ux
                ys += uy
                zs += uz
                n_ok += 1
        cra, cdec = ra_dec_from_unit((xs, ys, zs) if n_ok else (1.0, 0.0, 0.0))
        constellations.append({
            "name": name,
            "first": first,
            "count": len(all_segs) - first,
            "ra": cra,
            "dec": cdec,
        })

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("/* Generated by tools/gen_constellations.py — do not edit. */\n")
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("#define CONSTELLATION_COUNT %d\n" % len(constellations))
        f.write("#define CONSTELLATION_VERTEX_COUNT %d\n" % len(vertices))
        f.write("#define CONSTELLATION_SEG_COUNT %d\n\n" % len(all_segs))
        f.write("typedef struct {\n  uint16_t ra;\n  int16_t dec;\n} ConstVertex;\n\n")
        f.write("typedef struct {\n  uint16_t a;\n  uint16_t b;\n} ConstSeg;\n\n")
        f.write("typedef struct {\n  const char *name;\n  uint16_t first_seg;\n")
        f.write("  uint16_t n_seg;\n  uint16_t ra;\n  int16_t dec;\n} ConstellationRec;\n\n")
        f.write("static const ConstVertex CONSTELLATION_VERTS[CONSTELLATION_VERTEX_COUNT] = {\n")
        for ra, dec in vertices:
            f.write("  {%5d, %6d},\n" % (pack_ra(ra), pack_dec(dec)))
        f.write("};\n\n")
        f.write("static const ConstSeg CONSTELLATION_SEGS[CONSTELLATION_SEG_COUNT] = {\n")
        for a, b in all_segs:
            f.write("  {%4d, %4d},\n" % (a, b))
        f.write("};\n\n")
        f.write("static const ConstellationRec CONSTELLATIONS[CONSTELLATION_COUNT] = {\n")
        for c in constellations:
            f.write(
                '  {"%s", %d, %d, %d, %d},\n'
                % (c["name"], c["first"], c["count"], pack_ra(c["ra"]), pack_dec(c["dec"]))
            )
        f.write("};\n")

    print("Wrote %s" % OUT)
    print("  constellations: %d" % len(constellations))
    print("  vertices: %d" % len(vertices))
    print("  segments: %d" % len(all_segs))
    missing = set(IAU_NAMES.values()) - set(grouped)
    if missing:
        print("  missing: %s" % ", ".join(sorted(missing)))


if __name__ == "__main__":
    sys.exit(main() or 0)
