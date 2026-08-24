#!/usr/bin/env python3
"""Summarise an --oam-log capture: which OAM entries move with the D-pad.

Usage: python scripts/analyze-oam-log.py oam.oamlog
"""
import collections
import sys

KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN = 1 << 4, 1 << 5, 1 << 6, 1 << 7


def axis_signal(delta, held_positive, held_negative):
    """agree/oppose for one axis (x or y), given that axis's two direction keys.

    Only asserts a signal when one of the two keys for this axis is actually
    held; a held key whose direction contradicts the observed delta counts
    as "oppose", not merely "not agree".
    """
    agree = False
    oppose = False
    if held_positive:
        if delta > 0:
            agree = True
        elif delta < 0:
            oppose = True
    if held_negative:
        if delta < 0:
            agree = True
        elif delta > 0:
            oppose = True
    return agree, oppose


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

            x_agree, x_oppose = axis_signal(dx, keys & KEY_RIGHT, keys & KEY_LEFT)
            y_agree, y_oppose = axis_signal(dy, keys & KEY_DOWN, keys & KEY_UP)

            # On a diagonal hold (e.g. RIGHT+UP), crediting the row whenever
            # *either* axis matches over-credits sprites that only follow one
            # of the two held directions: a sprite moving dx>0 while UP is
            # contradicted (dy should be <0, not >0) is not a clean match.
            # A row only counts as agreement when no axis is contradicted;
            # any contradicted axis makes it a disagreement instead, even if
            # the other axis matched. Do not simplify this back to an OR of
            # per-axis agreement -- that reintroduces the over-crediting bug.
            if x_oppose or y_oppose:
                disagree[sig] += 1
            elif x_agree or y_agree:
                agree[sig] += 1

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
