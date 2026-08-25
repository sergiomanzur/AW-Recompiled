# Symbol tables

One INI file per ROM revision, named `<rom-sha1-lowercase>.ini`. The runtime
loads the file matching the ROM it booted and ignores any other. A missing or
mismatched file is not an error: the runtime falls back to correlation-based
indicator tracking, which needs no symbols.

INI rather than JSON because the project already ships `aw::ConfigFile` and
adding a JSON dependency for one small data file is not worth it.

## Format

    [Rom]
    sha1 = 15053499D5B3F49128A941D7F2D84876F5424D0C

    [MapView]
    predicate_addr   = 33556224   ; decimal EWRAM address (0x02000100)
    predicate_value  = 3
    predicate2_addr  = 0          ; optional second predicate, 0 = unused
    predicate2_value = 0
    indicator_tile    = 64        ; -1 = match any tile
    indicator_palette = -1        ; -1 = match any palette bank
    scroll_bg         = 1         ; BG layer whose scroll tracks content, -1 = none
    steerable         = 1         ; 0 disables pointer steering in this context

Recognised section names: `MapView`, `ListMenu`, `NameEntry`, `FrontEnd`,
`Cutscene`. A section with `predicate_addr = 0` (or absent) is skipped.

    [Cursor]
    x_addr = 50345636   ; 0x030036A4
    y_addr = 50345638   ; 0x030036A6

Absolute addresses (IWRAM or EWRAM) of the map cursor's tile X/Y coordinates,
mined by `aw-cursor-miner`. When present, `NavController` steers by exact
tile arithmetic instead of guessing a sprite's identity via `OamTracker`. A
table needs at least one of `[Cursor]` or a context section to load
successfully; a file with only `[Rom]` and `[Cursor]` (no context rules at
all) is valid on its own, since the mined cursor addresses are useful without
any context predicates.

Addresses are decimal because `ConfigFile::get_int` does not parse hex. Put the
hex value in a trailing comment.

Generate these with `aw-symbol-miner` (see `tools/`).

## Regenerating cursor coordinate addresses

The steering path moved from a sprite-position guess (`OamTracker`) to
reading the cursor's tile X/Y directly from RAM (see `[Cursor]` above).
Finding those addresses for a given screen (map view, list menu, etc.) is a
two-step, human-in-the-loop process:

1. Run the game normally (`advance-wars-native`), navigate to the screen you
   want to mine (e.g. the in-battle map cursor), and press **F5** while
   holding a digit key (`0`-`9`) to pick a save slot. This writes
   `state_<N>.ss` to the working directory and prints the filename to
   stdout. Holding no digit defaults to slot `0`. `state_*.ss` files are
   gitignored — they embed ROM-derived data and must never be committed.
2. Run the offline miner against that savestate:

       build/native/runtime/aw-cursor-miner "<rom path>" state_0.ss

   It presses Right/Left/Down/Up against the loaded state several times,
   diffs EWRAM (and IWRAM, when the core exposes it) before/after each
   press, and reports every byte offset that moved by exactly +/-1 in
   lockstep with the corresponding direction. Byte offsets adjacent in
   memory are called out separately as likely X/Y pairs — cursor coordinates
   are usually stored next to each other.
3. Record the winning **absolute** address (e.g. `0x02001234`) as a new
   symbol entry below, in **decimal**, with the hex value in a trailing
   comment — same convention as every other address in this file, because
   `ConfigFile::get_int` only parses base-10.

If the miner reports no candidates for an axis, it is telling you the truth
rather than fabricating one: recapture the savestate on a screen where the
D-pad is known to move a cursor, away from a map edge, and not mid-animation,
then try again.

## Mining beyond the cursor: aw-boot-probe

`runtime/tools/boot_probe.cpp` is the headless mining harness. It boots the
real core with scripted input and diffs memory (and, where useful, the
framebuffer) without ever writing game state. Modes:

    aw-boot-probe <rom> boot [--out state.ss] [--max-frames N]
        Spam Start/A through the front-end until the map cursor answers the
        D-pad, then save a savestate. Writes bootdump/frame_*.bmp screen
        dumps, and state_stuck.ss (the screen it stalled on) for the other
        modes.

    aw-boot-probe <rom> rawprobe <state.ss> "<script>"
        Replay a key sequence (U/D/L/R/A/B/S, '.' = idle) against a
        savestate, printing every EWRAM/IWRAM/OAM byte that changed after
        each press plus a screenshot per press (rawshot_NNN_<key>.bmp).

    aw-boot-probe <rom> gridprobe <state.ss>
        Find a grid-cursor variable: bytes tracing a clean two-steps-out,
        two-steps-back walk under R,R,L,L,U,U,D,D.

    aw-boot-probe <rom> namepass <state.ss>
    aw-boot-probe <rom> tomap <state.ss> [out.ss]
        Escape the boot-time name entry (the wrap-cycling sequence below)
        and, for tomap, march on toward the map.

### What is verified so far (Advance Wars USA Rev 1)

| Address        | Meaning                                             | How verified |
| -------------- | --------------------------------------------------- | ------------ |
| 0x030036A4/A6  | Map cursor tile X/Y (already in `[Cursor]` above)   | aw-cursor-miner + daily pointer steering |
| 0x02008032     | Frame tick counter (+1 per emulated frame)          | rawprobe: +8 per 8-frame scripted press |
| 0x02011CEA/B   | 16-bit RNG seed (churns on input)                   | rawprobe: uncorrelated jumps per press |
| (behaviour)    | Name-entry grid WRAPS; Right/Down cycle all cells   | rawprobe sweep + visual verification |
| (behaviour)    | 80xRight + 80xDown + A escapes name entry          | namepass: screen transition verified |
| (behaviour)    | Portrait animation repaints the full screen every few frames, so naive pixel-diff detectors false-positive | tomap stage-1 debugging |

Still unmined (the sidebar/undo features degrade gracefully without them):
ListMenu open/cursor predicate (for `[ListMenu]`), day counter, funds, CO
power meters, the unit table. The fastest path for those is the human one
from the previous section: play to the screen of interest, F5 a savestate,
then `rawprobe` scripted transitions against it.

### Engineering note: honesty over invention

The sidebar and undo features are built on reads that are verified, and
report `-` / hidden for everything else. When a new address is mined,
record it here with how it was verified, then wire it into
`runtime/src/tactical_intel.cpp` behind a validity flag - never a fallback
that invents a plausible value.
