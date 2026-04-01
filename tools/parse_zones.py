#!/usr/bin/env python3
"""
parse_zones.py — reads GTA III's gta3.zon and outputs a territories.txt
for III.GangTerritoryWars based on a known zone-to-gang mapping.

Usage:
  python tools/parse_zones.py [--zon <path>] [--out <path>]

Defaults:
  --zon  F:/GTA/GTA III - Remastered/data/gta3.zon
  --out  data/territories.txt

Zone format in gta3.zon:
  NAME, type, x1, y1, z1, x2, y2, z2, island_level

Only type-0 zones (navigation/neighborhood zones) are used.
type-1 = roadblock, type-2 = trigger zones (hospitals, police etc.)
island_level: 1=Portland, 2=Staunton, 3=Shoreside
"""

import argparse
import sys
import os

# ---------------------------------------------------------------------------
# Zone-to-gang assignment table.
# gangCode matches ePedType values used by III.GangTerritoryWars:
#   7=Mafia  8=Triads  9=Diablos  10=Yakuza  11=Colombians  12=Yardies
# defenseLevel: 0=easy  1=medium  2=boss/stronghold
#
# Zones NOT listed here are excluded from the output (bridges, airports, etc.)
# ---------------------------------------------------------------------------
ZONE_ASSIGNMENTS = {
    # --- Portland (Act 1) ---
    "CHINA":   {"gang": 8,  "defense": 2, "label": "Chinatown (boss)"},
    "REDLIGH": {"gang": 8,  "defense": 1, "label": "Red Light District"},
    "TOWERS":  {"gang": 9,  "defense": 2, "label": "Hepburn Heights (boss)"},
    "HARWOOD": {"gang": 9,  "defense": 1, "label": "Harwood"},
    "LITTLEI": {"gang": 7,  "defense": 2, "label": "Little Italy / St. Mark's (boss)"},
    "S_VIEW":  {"gang": 7,  "defense": 1, "label": "Portland View"},
    "EASTBAY": {"gang": 7,  "defense": 0, "label": "Portland Beach"},

    # --- Staunton Island (Act 2) ---
    "YAKUSA":  {"gang": 10, "defense": 2, "label": "Torrington (boss)"},
    "COM_EAS": {"gang": 10, "defense": 1, "label": "Newport / Commercial East"},
    "SHOPING": {"gang": 11, "defense": 1, "label": "Rockford / Shopping area"},
    "PARK":    {"gang": 11, "defense": 2, "label": "Belleville Park (boss)"},
    "CONSTRU": {"gang": 12, "defense": 1, "label": "Fort Staunton (construction)"},
    "STADIUM": {"gang": 12, "defense": 2, "label": "Rockford Stadium (boss)"},
    "HOSPI_2": {"gang": 12, "defense": 0, "label": "North Staunton / Hospital"},

    # --- Shoreside Vale (Act 3) ---
    "PROJECT": {"gang": 12, "defense": 2, "label": "Wichita Gardens (boss)"},
    "SWANKS":  {"gang": 11, "defense": 2, "label": "Cedar Grove (boss)"},
    "SUB_IND": {"gang": 12, "defense": 1, "label": "Shoreside industrial"},
    "BIG_DAM": {"gang": 11, "defense": 1, "label": "Cochrane Dam"},
}

GANG_NAMES = {7: "Mafia", 8: "Triads", 9: "Diablos",
              10: "Yakuza", 11: "Colombians", 12: "Yardies"}

ISLAND_NAMES = {1: "PORTLAND", 2: "STAUNTON", 3: "SHORESIDE"}

# ---------------------------------------------------------------------------

def parse_zon(path):
    zones = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.lower() in ("zone", "end"):
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) < 9:
                continue
            try:
                name = parts[0]
                ztype = int(parts[1])
                x1, y1 = float(parts[2]), float(parts[3])
                x2, y2 = float(parts[5]), float(parts[6])
                level = int(parts[8])
                zones.append({
                    "name": name,
                    "type": ztype,
                    "minX": min(x1, x2),
                    "minY": min(y1, y2),
                    "maxX": max(x1, x2),
                    "maxY": max(y1, y2),
                    "level": level,
                })
            except (ValueError, IndexError):
                continue
    return zones


def generate_id(zone_name, island_level, gang_code):
    """Derive a stable 4-digit territory ID from zone + gang."""
    # Use first 2 digits for island (1x, 2x, 3x) and a hash-derived suffix.
    prefix = island_level * 1000
    # Map zone name to a deterministic offset using gang + hash.
    offset = (abs(hash(zone_name)) % 90) * 10 + gang_code % 10
    return prefix + offset


def main():
    parser = argparse.ArgumentParser(description="Build territories.txt from gta3.zon")
    parser.add_argument("--zon",
        default="F:/GTA/GTA III - Remastered/data/gta3.zon",
        help="Path to gta3.zon")
    parser.add_argument("--out",
        default=os.path.join(os.path.dirname(__file__), "..", "data", "territories.txt"),
        help="Output path for territories.txt")
    parser.add_argument("--list", action="store_true",
        help="Just print all type-0 zones and exit (useful for exploring new zones)")
    args = parser.parse_args()

    zones = parse_zon(args.zon)

    if args.list:
        print(f"{'Zone':<12} {'Island':<12} {'minX':>8} {'minY':>8} {'maxX':>8} {'maxY':>8}")
        print("-" * 60)
        for z in zones:
            if z["type"] != 0:
                continue
            iname = ISLAND_NAMES.get(z["level"], f"L{z['level']}")
            print(f"{z['name']:<12} {iname:<12} {z['minX']:>8.1f} {z['minY']:>8.1f} "
                  f"{z['maxX']:>8.1f} {z['maxY']:>8.1f}")
        return

    # Collect assigned zones grouped by island
    by_island = {1: [], 2: [], 3: []}
    used_ids = set()
    for z in zones:
        if z["type"] != 0:
            continue
        if z["name"] not in ZONE_ASSIGNMENTS:
            continue
        assign = ZONE_ASSIGNMENTS[z["name"]]
        tid = generate_id(z["name"], z["level"], assign["gang"])
        while tid in used_ids:
            tid += 1
        used_ids.add(tid)
        by_island.setdefault(z["level"], []).append({
            "id": tid,
            "minX": z["minX"],
            "minY": z["minY"],
            "maxX": z["maxX"],
            "maxY": z["maxY"],
            "gang": assign["gang"],
            "defense": assign["defense"],
            "label": assign["label"],
            "zone": z["name"],
        })

    # Sort within each island by gang then minY
    for entries in by_island.values():
        entries.sort(key=lambda e: (e["gang"], e["minY"]))

    # Write output
    lines = []
    lines.append("# id,minX,minY,maxX,maxY,ownerGangCode,underAttack,defenseLevel")
    lines.append("# ownerGangCode: 7=Mafia  8=Triads  9=Diablos  10=Yakuza  11=Colombians  12=Yardies")
    lines.append("# defenseLevel: 0=easy  1=medium  2=boss (gang stronghold)")
    lines.append("# Generated by tools/parse_zones.py from gta3.zon")
    lines.append("")

    island_headers = {
        1: "PORTLAND  (Act 1 — x > 616)",
        2: "STAUNTON ISLAND  (Act 2 — -378 < x <= 616)",
        3: "SHORESIDE VALE  (Act 3 — x <= -378)",
    }

    for level in [1, 2, 3]:
        entries = by_island.get(level, [])
        if not entries:
            continue
        lines.append(f"# {'=' * 60}")
        lines.append(f"# {island_headers[level]}")
        lines.append(f"# {'=' * 60}")

        current_gang = None
        for e in entries:
            if e["gang"] != current_gang:
                current_gang = e["gang"]
                lines.append(f"")
                lines.append(f"# --- {GANG_NAMES[e['gang']]} ({e['gang']}) ---")

            row = (f"{e['id']},"
                   f"{e['minX']:.0f},{e['minY']:.0f},"
                   f"{e['maxX']:.0f},{e['maxY']:.0f},"
                   f"{e['gang']},0,{e['defense']}"
                   f"  # {e['zone']}: {e['label']}")
            lines.append(row)
        lines.append("")

    out_path = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    print(f"Wrote {len(used_ids)} territories to {out_path}")

    # Print summary
    gang_counts = {}
    for entries in by_island.values():
        for e in entries:
            gang_counts[e["gang"]] = gang_counts.get(e["gang"], 0) + 1
    for gc, cnt in sorted(gang_counts.items()):
        print(f"  {GANG_NAMES[gc]}: {cnt} territories")


if __name__ == "__main__":
    main()
