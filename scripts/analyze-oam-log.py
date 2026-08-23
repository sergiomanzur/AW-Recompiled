#!/usr/bin/env python3
"""Summarise an --oam-log capture: which OAM entries move with the D-pad.

Usage: python scripts/analyze-oam-log.py oam.oamlog
"""
import collections
import sys

KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN = 1 << 4, 1 << 5, 1 << 6, 1 << 7


def main(path):
    # (tile, palette) -> counts of moves that agreed / disagreed with the D-pad
    agree = collections.Counter()
    disagree = collections.Counter()
    seen = collections.Counter()

    with open(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 9:
                continue
            _frame, keys, _index, dx, dy, _x, _y, tile, palette = (int(p) for p in parts)
            sig = (tile, palette)
            seen[sig] += 1
            if keys & KEY_RIGHT and dx > 0 or keys & KEY_LEFT and dx < 0 \
               or keys & KEY_DOWN and dy > 0 or keys & KEY_UP and dy < 0:
                agree[sig] += 1
            elif keys & (KEY_RIGHT | KEY_LEFT | KEY_UP | KEY_DOWN):
                disagree[sig] += 1

    rows = []
    for sig, total in seen.items():
        a, d = agree[sig], disagree[sig]
        score = a - d
        rows.append((score, a, d, total, sig))
    rows.sort(reverse=True)

    print(f"{'score':>6} {'agree':>6} {'disagr':>6} {'moves':>6}  tile  palette")
    for score, a, d, total, (tile, palette) in rows[:20]:
        print(f"{score:>6} {a:>6} {d:>6} {total:>6}  0x{tile:03X}  {palette}")

    if not rows:
        print("No moving on-screen sprites recorded. The indicator is probably")
        print("a background tile, not a sprite: use the BG-tilemap fallback.")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "oam.oamlog")
