#!/usr/bin/env python3
"""
map_viz.py — overlays territories.txt onto the GTA III map image.

Usage:
  python tools/map_viz.py [--map <image>] [--territories <file>]
                          [--zon <file>] [--out <output>]
                          [--zones]  # draw gta3.zon outlines for reference

Defaults:
  --map          C:/Users/nnoce/.claude/image-cache/04e34c6e-632f-48f7-8d9f-03d90d531379/2.png
  --territories  data/territories.txt
  --zon          F:/GTA/GTA III - Remastered/data/gta3.zon
  --out          tools/territory_map.png

World-to-pixel calibration is derived by scanning the map image for
non-black pixels, then linearly mapping the known world bounds to those
pixel bounds. Adjust WORLD_* constants below if calibration drifts.
"""

import argparse
import os
import re
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow not installed. Run: pip install Pillow")

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

# ---------------------------------------------------------------------------
# World coordinate extent (from gta3.zon superzones + some padding)
# ---------------------------------------------------------------------------
WORLD_MIN_X = -1645.0
WORLD_MAX_X =  1903.0
WORLD_MIN_Y = -1720.0   # south (bottom of image)
WORLD_MAX_Y =  1206.0   # north (top of image)

# ---------------------------------------------------------------------------
# Gang display config — colors match in-game radar
# ---------------------------------------------------------------------------
GANGS = {
    7:  {"name": "Mafia",      "color": (60,  220, 60,  140)},
    8:  {"name": "Triads",     "color": (60,  60,  235, 140)},
    9:  {"name": "Diablos",    "color": (245, 60,  60,  140)},
    10: {"name": "Yakuza",     "color": (60,  200, 230, 140)},
    11: {"name": "Colombians", "color": (220, 60,  200, 140)},
    12: {"name": "Yardies",    "color": (60,  230, 200, 140)},
}

# Zones to annotate from gta3.zon when --zones flag is used
NAMED_ZONES = {
    "CHINA", "REDLIGH", "TOWERS", "HARWOOD", "LITTLEI",
    "S_VIEW", "EASTBAY", "PORT_W", "PORT_S", "PORT_E", "PORT_I",
    "YAKUSA", "COM_EAS", "SHOPING", "PARK", "CONSTRU", "STADIUM", "HOSPI_2",
    "AIRPORT", "PROJECT", "SWANKS", "SUB_IND", "BIG_DAM",
}

# ---------------------------------------------------------------------------

def find_content_bounds(img_path):
    """Scan image for non-black pixels to find the actual map content area."""
    img = Image.open(img_path).convert("RGB")
    if HAS_NUMPY:
        import numpy as np
        arr = np.array(img)
        mask = arr.sum(axis=2) > 30
        rows = np.any(mask, axis=1)
        cols = np.any(mask, axis=0)
        rmin = int(np.where(rows)[0][0])
        rmax = int(np.where(rows)[0][-1])
        cmin = int(np.where(cols)[0][0])
        cmax = int(np.where(cols)[0][-1])
    else:
        w, h = img.size
        pixels = img.load()
        cmin = w; cmax = 0; rmin = h; rmax = 0
        for y in range(h):
            for x in range(w):
                r, g, b = pixels[x, y]
                if r + g + b > 30:
                    cmin = min(cmin, x); cmax = max(cmax, x)
                    rmin = min(rmin, y); rmax = max(rmax, y)
    return cmin, rmin, cmax, rmax


def world_to_pixel(wx, wy, px_left, px_top, px_right, px_bottom):
    """Map a world (x, y) coordinate to image pixel (px, py)."""
    sx = (px_right - px_left) / (WORLD_MAX_X - WORLD_MIN_X)
    sy = (px_bottom - px_top) / (WORLD_MAX_Y - WORLD_MIN_Y)
    px = px_left  + (wx - WORLD_MIN_X) * sx
    py = px_bottom - (wy - WORLD_MIN_Y) * sy   # Y is inverted (world up = pixel up)
    return int(px), int(py)


def parse_territories(path):
    territories = []
    with open(path, "r") as f:
        for line in f:
            line = line.split("#")[0].strip()
            if not line:
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) < 6:
                continue
            try:
                tid = parts[0]
                minX, minY, maxX, maxY = float(parts[1]), float(parts[2]), float(parts[3]), float(parts[4])
                gang = int(parts[5])
                defense = int(parts[7]) if len(parts) > 7 else 0
                territories.append({"id": tid, "minX": minX, "minY": minY,
                                     "maxX": maxX, "maxY": maxY,
                                     "gang": gang, "defense": defense})
            except (ValueError, IndexError):
                continue
    return territories


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
                zones.append({"name": name, "type": ztype,
                               "minX": min(x1,x2), "minY": min(y1,y2),
                               "maxX": max(x1,x2), "maxY": max(y1,y2)})
            except (ValueError, IndexError):
                continue
    return zones


def draw_label(draw, text, cx, cy, font, color):
    """Draw centered text with a dark drop shadow."""
    for dx, dy in [(-1,0),(1,0),(0,-1),(0,1)]:
        draw.text((cx+dx, cy+dy), text, font=font, fill=(0,0,0,220), anchor="mm")
    draw.text((cx, cy), text, font=font, fill=color, anchor="mm")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)

    parser = argparse.ArgumentParser(description="Visualize gang territories on GTA III map")
    parser.add_argument("--map",
        default=r"C:/Users/nnoce/.claude/image-cache/04e34c6e-632f-48f7-8d9f-03d90d531379/2.png")
    parser.add_argument("--territories",
        default=os.path.join(repo, "data", "territories.txt"))
    parser.add_argument("--zon",
        default="F:/GTA/GTA III - Remastered/data/gta3.zon")
    parser.add_argument("--out",
        default=os.path.join(here, "territory_map.png"))
    parser.add_argument("--zones", action="store_true",
        help="Draw gta3.zon zone outlines as reference")
    args = parser.parse_args()

    print(f"Loading map: {args.map}")
    base = Image.open(args.map).convert("RGBA")
    W, H = base.size

    print("Detecting content bounds...")
    px_left, px_top, px_right, px_bottom = find_content_bounds(args.map)
    print(f"  Content area: x=[{px_left},{px_right}]  y=[{px_top},{px_bottom}]")

    def w2p(wx, wy):
        return world_to_pixel(wx, wy, px_left, px_top, px_right, px_bottom)

    # Overlay layer (RGBA for transparency)
    overlay = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    # --- Optional: draw zone outlines from gta3.zon ---
    if args.zones and os.path.exists(args.zon):
        print("Drawing zone reference outlines...")
        zones = parse_zon(args.zon)
        for z in zones:
            if z["type"] != 0 or z["name"] not in NAMED_ZONES:
                continue
            p1 = w2p(z["minX"], z["maxY"])
            p2 = w2p(z["maxX"], z["minY"])
            draw.rectangle([p1, p2], outline=(255, 255, 255, 80), width=1)

    # --- Draw territories ---
    print(f"Loading territories: {args.territories}")
    territories = parse_territories(args.territories)
    print(f"  {len(territories)} territories found")

    try:
        font_large = ImageFont.truetype("arial.ttf", 14)
        font_small = ImageFont.truetype("arial.ttf", 11)
    except OSError:
        font_large = ImageFont.load_default()
        font_small = font_large

    for t in territories:
        gang = t["gang"]
        cfg = GANGS.get(gang, {"name": f"Gang{gang}", "color": (200,200,200,120)})
        r, g, b, a = cfg["color"]

        # Thicker border for boss territories
        border_w = 3 if t["defense"] == 2 else 1
        border_color = (255, 255, 255, 200) if t["defense"] == 2 else (r, g, b, 200)

        # top-left and bottom-right pixel coords
        # world minX,maxY → top-left pixel; world maxX,minY → bottom-right pixel
        tl = w2p(t["minX"], t["maxY"])
        br = w2p(t["maxX"], t["minY"])

        # Fill
        draw.rectangle([tl, br], fill=(r, g, b, a))
        # Border
        draw.rectangle([tl, br], outline=border_color, width=border_w)

        # Label
        cx = (tl[0] + br[0]) // 2
        cy = (tl[1] + br[1]) // 2
        label = cfg["name"][:3].upper()
        if t["defense"] == 2:
            label += "★"
        draw.text((cx+1, cy+1), label, font=font_small, fill=(0,0,0,200), anchor="mm")
        draw.text((cx, cy), label, font=font_small, fill=(255,255,255,230), anchor="mm")

    # --- Legend ---
    legend_x, legend_y = W - 200, 20
    draw.rectangle([legend_x - 8, legend_y - 8,
                    legend_x + 190, legend_y + len(GANGS) * 22 + 10],
                   fill=(0, 0, 0, 180), outline=(200, 200, 200, 200), width=1)
    for i, (code, cfg) in enumerate(sorted(GANGS.items())):
        r, g, b, a = cfg["color"]
        y = legend_y + i * 22
        draw.rectangle([legend_x, y, legend_x + 16, y + 16], fill=(r, g, b, 220))
        draw.text((legend_x + 22, y + 2), f"{cfg['name']} ({code})",
                  font=font_small, fill=(255, 255, 255, 230))

    draw.text((legend_x, legend_y + len(GANGS) * 22 - 4),
              "★ = boss territory", font=font_small, fill=(200, 200, 200, 180))

    # Composite and save
    result = Image.alpha_composite(base, overlay)
    out_path = os.path.abspath(args.out)
    result.convert("RGB").save(out_path)
    print(f"Saved: {out_path}")

    # Print territory summary
    from collections import Counter
    counts = Counter(GANGS.get(t["gang"], {"name": f"Gang{t['gang']}"})["name"]
                     for t in territories)
    for name, cnt in sorted(counts.items()):
        print(f"  {name}: {cnt}")


if __name__ == "__main__":
    main()
