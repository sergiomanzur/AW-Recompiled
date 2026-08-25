# Recomp assets — gba-recomp pipeline state

Artifacts for the static-recompilation migration path
(https://github.com/JRickey/gba-recomp), separate from the mGBA runtime the
game currently boots through.

## Files

- `aw.labels.toml` — the function map for Advance Wars (USA, Rev 1):
  addresses and modes only, no game bytes, safe to share/commit. Grown by
  runtime label recording (see below), union-merge safe.
- `aw-soak-input.txt` — `gba-input v1` script (active-low KEYINPUT masks)
  that drives the game through boot, menus, the name-entry screen and deep
  game code for headless soak runs.

## Verified state (2026-08-25, Windows, lld-link + MinGW GCC)

| Step | Result |
| --- | --- |
| Toolkit build | `RUSTFLAGS="-C linker=lld-link" cargo build --release -p recomp` — clean, 8m27s |
| First recomp (static analysis only) | 2,227 blocks -> ~925 KB native DLL; boot runs ~24% native execution |
| 600-frame label soak | +436 ROM / +77 IWRAM entries; rebuild -> 3,034 blocks, ~81% native on the boot path |
| 20,000-frame soak with `aw-soak-input.txt` | +8,424 ROM / +36 IWRAM entries (now 8,860 ROM + 113 IWRAM total) |
| Differential verify, boot (600 frames) | `verify MATCH` — recomp == interpreter frame hash |
| Differential verify, deep (20,000 frames, gameplay input) | `verify MATCH` |
| Interactive play (`recomp play --stats --record-labels`) | **Playable.** Profiling boot found 9,500 RAM entry points; translation grew to **37,011 blocks / 3.6M instructions**. Booted BIOS-HLE, loaded the battery save, auto-detected the audio engine (M4A/MP2K, HLE shadow armed), and reached the mission map via injected keyboard input: menu navigation (Down/Enter) and dialogue advance (A-mash) both confirmed by pixel-diffed screen captures (~184k pixels changed per input round). Their frontend reported ~16.5 ms/frame on this laptop (at the 60 fps budget); the audio-ring backlog warning is a quirk of their cpal frontend — irrelevant to embedding, where our waveOut pipeline consumes their APU samples directly. Battery `.sav` load/save both work. |

"Native %" figures mix units (native block entries vs interpreter steps)
and vary by workload; the invariant that matters is `verify MATCH`.

Note: the play path's profiling-boot seeds (the 8,557 extra ROM entries
behind the 37k-block build) live in the per-user translation cache, not the
portable labels file — a fresh `build`/`verify` uses the 8,860+113 entries
exported here; `play` re-profiles on first launch automatically.

## Reproducing / growing coverage

```sh
cd <gba-recomp checkout>
export PATH="$PATH:~/.cargo/bin:~/scoop/apps/gcc/current/bin"
export GBA_RECOMP_CC=~/scoop/apps/gcc/current/bin/gcc.exe   # POSIX-style cc; MSVC clang rejects -fPIC
export RECOMP_JOBS=8

# Restore this repo's labels (addresses only) into the local accumulator:
./target/release/recomp labels export   # (they live in AppData once imported)
./target/release/recomp labels import <rom> ../advance-wars-recomp/data/recomp/aw.labels.toml

# The coverage loop - repeat until new-entry counts converge to zero:
./target/release/recomp build  "<rom>"
./target/release/recomp runc   "<rom>" --frames 20000 --input ../advance-wars-recomp/data/recomp/aw-soak-input.txt --record-labels
./target/release/recomp verify "<rom>" --frames 20000 --input ../advance-wars-recomp/data/recomp/aw-soak-input.txt

# Publish the grown map back into this repo:
./target/release/recomp labels export "<rom>" ../advance-wars-recomp/data/recomp/aw.labels.toml
```

IWRAM entries also need the local `<sha>.iwram` snapshot (captured
automatically during `--record-labels` runs; machine-local by design since
it contains game bytes).

## Migration plan (into this runtime)

1. **Converge coverage** with the loop above, adding soak inputs that reach
   battle/map screens (needs either deeper scripted input or a live
   `play --record-labels` session).
2. **Embed the runtime**: the hardware model is a Rust C-ABI library
   (scheduler, PPU, APU, DMA/timers/IRQ, savestates). Link it plus the
   recompiled game DLL behind an `aw_recomp_adapter` implementing the same
   contract as the mGBA adapter (`run_frame(keys)`, memory block pointers,
   audio drain, reset, savestate capture/restore). The CLI's `runc` path is
   the reference host.
3. **Port the feature surface**: rewind ring + undo stack onto their
   savestates (measure snapshot cost against our ~3/s, ~450 KB cadence);
   verify replay determinism on the recomp side; probes keep working via
   the memory-pointer seam.
4. **Differential-soak both cores** (mGBA vs recomp) using this repo's
   `--replay` files as shared input scripts before flipping the default.
