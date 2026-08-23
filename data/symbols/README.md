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

Addresses are decimal because `ConfigFile::get_int` does not parse hex. Put the
hex value in a trailing comment.

Generate these with `aw-symbol-miner` (see `tools/`).
