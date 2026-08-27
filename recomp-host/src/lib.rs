//! aw_recomp_host — embeds the gba-recomp static recompilation runtime
//! behind the same C contract our mGBA adapter exposes (aw_mgba_*), so the
//! entire AW-Recompiled feature surface (probes, rewind, undo, replays,
//! sidebar, cheats) runs unchanged on the native backend.
//!
//! The host loads the recompiled game DLL (`rcg_blocks` table, produced by
//! `recomp build` or the play launcher's first-launch translation) and
//! drives it with the same dispatch loop the upstream CLI uses: translated
//! blocks where available, the interpreter for the rest, IRQ machinery at
//! block boundaries, frame-terminated.
//!
//! Savestates are Machine clones (gba-recomp patched with Clone derives);
//! the decode cache resets on snapshot/restore - it is memoization keyed
//! by ROM content, so a fresh one is always safe.

use gba_core::capi::{RcgBlock, RT_API};
use gba_core::machine::{Machine, IRQ_RETURN_ADDR};
use gba_core::cpu::FLAG_I;
use gba_core::Bus as _;
use libloading::Library;
use std::ffi::c_void;
use std::path::PathBuf;

const ROM_BASE: u32 = 0x0800_0000;
const IWRAM_BASE: u32 = 0x0300_0000;
const EWRAM_BASE: u32 = 0x0200_0000;
const FRAME_STEP_GUARD: u64 = 5_000_000;

type BlockFn = extern "C" fn(*const gba_core::capi::RtApi, *mut c_void) -> u32;

extern "C" fn default_step_fn(_api: *const gba_core::capi::RtApi, m: *mut c_void) -> u32 {
    let mach = unsafe { &mut *(m as *mut Machine) };
    mach.step();
    0
}

pub struct BlockTable {
    rom: Vec<BlockFn>,
    iwram: Vec<BlockFn>,
    ewram: Vec<BlockFn>,
    len: usize,
}

impl BlockTable {
    fn load(lib: &Library) -> Result<BlockTable, String> {
        unsafe {
            let blocks_ptr: *const RcgBlock = {
                let sym: libloading::Symbol<*const RcgBlock> =
                    lib.get(b"rcg_blocks").map_err(|e| e.to_string())?;
                *sym
            };
            let count: usize = {
                let sym: libloading::Symbol<*const u64> =
                    lib.get(b"rcg_block_count").map_err(|e| e.to_string())?;
                **sym as usize
            };
            let blocks = std::slice::from_raw_parts(blocks_ptr, count);

            let mut rom_max = 0usize;
            for b in blocks {
                let r = (b.key & 0x01FF_FFFF) as usize;
                if b.key >> 24 >= 8 && b.key >> 24 <= 0xD {
                    rom_max = rom_max.max(r + 1);
                }
            }
            let mut t = BlockTable {
                rom: vec![default_step_fn; rom_max],
                iwram: vec![default_step_fn; 0x8000],
                ewram: vec![default_step_fn; 0x4_0000],
                len: blocks.len(),
            };
            for b in blocks {
                match b.key >> 24 {
                    0x08..=0x0D => {
                        let r = (b.key & 0x01FF_FFFF) as usize;
                        if r < t.rom.len() {
                            t.rom[r] = b.func;
                        }
                    }
                    0x03 => {
                        let w = (b.key & 0x7FFF) as usize;
                        if w < t.iwram.len() {
                            t.iwram[w] = b.func;
                        }
                    }
                    0x02 => {
                        let e = (b.key & 0x3_FFFF) as usize;
                        if e < t.ewram.len() {
                            t.ewram[e] = b.func;
                        }
                    }
                    _ => {}
                }
            }
            Ok(t)
        }
    }

    #[inline(always)]
    fn get(&self, key: u32) -> BlockFn {
        match key >> 24 {
            0x08..=0x0D => {
                let r = (key & 0x01FF_FFFF) as usize;
                if r < self.rom.len() {
                    unsafe { *self.rom.get_unchecked(r) }
                } else {
                    default_step_fn
                }
            }
            0x03 => {
                let w = (key & 0x7FFF) as usize;
                if w < self.iwram.len() {
                    unsafe { *self.iwram.get_unchecked(w) }
                } else {
                    default_step_fn
                }
            }
            0x02 => {
                let e = (key & 0x3_FFFF) as usize;
                if e < self.ewram.len() {
                    unsafe { *self.ewram.get_unchecked(e) }
                } else {
                    default_step_fn
                }
            }
            _ => default_step_fn,
        }
    }
}

pub struct Host {
    pub machine: Machine,
    pub rom: Vec<u8>,
    pub table: BlockTable,
    _lib: Library, // keeps the block functions alive
    pub video: *mut u32,
    pub native_blocks: u64,
    pub fallback_steps: u64,
}

fn convert_frame(src: &[u16], dst: *mut u32) {
    if dst.is_null() {
        return;
    }
    // GBA BGR555 (xbbbbbgggggrrrrr) -> the layout the mGBA adapter handed
    // the runtime: low byte = red, so the existing R/B swap in the copy
    // path produces RGB in the window framebuffer.
    for (i, &px) in src.iter().enumerate() {
        let r = ((px & 0x1F) as u32) << 3 | ((px & 0x1F) as u32 >> 2);
        let g = (((px >> 5) & 0x1F) as u32) << 3 | (((px >> 5) & 0x1F) as u32 >> 2);
        let b = (((px >> 10) & 0x1F) as u32) << 3 | (((px >> 10) & 0x1F) as u32 >> 2);
        unsafe {
            *dst.add(i) = 0xFF00_0000u32 | r | (g << 8) | (b << 16);
        }
    }
}

#[no_mangle]
pub extern "C" fn aw_recomp_create(rom_path: *const i8, dll_path: *const i8, video: *mut u32) -> *mut Host {
    use std::ffi::CStr;
    let rom_path = unsafe { CStr::from_ptr(rom_path) }.to_string_lossy().into_owned();
    let dll_path = unsafe { CStr::from_ptr(dll_path) }.to_string_lossy().into_owned();

    let rom = match std::fs::read(&rom_path) {
        Ok(b) => b,
        Err(e) => {
            eprintln!("aw-recomp-host: failed to read ROM at {rom_path}: {e}");
            return std::ptr::null_mut();
        }
    };

    let lib = match unsafe { Library::new(&dll_path) } {
        Ok(l) => l,
        Err(e) => {
            eprintln!("aw-recomp-host: failed to load game DLL at {dll_path}: {e}");
            return std::ptr::null_mut();
        }
    };

    let table = match BlockTable::load(&lib) {
        Ok(t) => t,
        Err(e) => {
            eprintln!("aw-recomp-host: corrupt/incompatible block table in {dll_path}: {e}");
            return std::ptr::null_mut();
        }
    };

    println!(
        "aw-recomp-host: {} translated blocks loaded from {}",
        table.len, dll_path
    );
    Box::into_raw(Box::new(Host {
        machine: Machine::new(rom.clone()),
        rom,
        table,
        _lib: lib,
        video,
        native_blocks: 0,
        fallback_steps: 0,
    }))
}

#[no_mangle]
pub extern "C" fn aw_recomp_destroy(h: *mut Host) {
    if !h.is_null() {
        unsafe { drop(Box::from_raw(h)) };
    }
}

#[no_mangle]
pub extern "C" fn aw_recomp_run_frame(h: *mut Host, keys: u16) {
    let h = unsafe { &mut *h };
    let m = &mut h.machine;
    // Our runtime speaks active-high keys; KEYINPUT is active-low.
    m.bus.keys = !keys & 0x03FF;
    m.bus.frame_ready = false;
    let mptr = m as *mut Machine as *mut c_void;

    let t_start = std::time::Instant::now();
    let mut block_duration = std::time::Duration::ZERO;
    let mut step_duration = std::time::Duration::ZERO;
    let mut steps = 0u64;
    while !m.bus.frame_ready && steps < FRAME_STEP_GUARD {
        steps += 1;
        if m.bus.halted
            || (!m.bus.real_bios
                && m.cpu.regs[15] == IRQ_RETURN_ADDR
                && m.cpu.mode() == gba_core::Mode::Irq)
            || (m.bus.irq_pending() && !m.cpu.flag(FLAG_I))
        {
            let t0 = std::time::Instant::now();
            m.step();
            step_duration += t0.elapsed();
            continue;
        }
        let key = m.cpu.regs[15] | m.cpu.thumb() as u32;
        if let Some(hk) = m.bus.mp2k.as_deref() {
            if hk.active && hk.hook_match(key) {
                m.bus.mp2k_frame_hook(key);
            }
        }
        let f = h.table.get(key);
        let t0 = std::time::Instant::now();
        f(&RT_API, mptr);
        block_duration += t0.elapsed();
    }

    let t_conv = std::time::Instant::now();
    convert_frame(&m.bus.framebuffer, h.video);
    let conv_duration = t_conv.elapsed();
    let total_duration = t_start.elapsed();

    static mut FRAME_COUNTER: u32 = 0;
    unsafe {
        FRAME_COUNTER += 1;
        if FRAME_COUNTER <= 5 || FRAME_COUNTER % 30 == 0 {
            eprintln!(
                "Frame {}: total={:.2}ms, blocks={:.2}ms (steps={}), m.step={:.2}ms, convert={:.2}ms",
                FRAME_COUNTER,
                total_duration.as_secs_f64() * 1000.0,
                block_duration.as_secs_f64() * 1000.0,
                steps,
                step_duration.as_secs_f64() * 1000.0,
                conv_duration.as_secs_f64() * 1000.0
            );
        }
    }
}

#[no_mangle]
pub extern "C" fn aw_recomp_read_audio(h: *mut Host, out: *mut i16, max_pairs: usize) -> usize {
    let h = unsafe { &mut *h };
    let buf = &mut h.machine.bus.audio_buf;
    let pairs = buf.len() / 2;
    let take = pairs.min(max_pairs);
    unsafe {
        std::ptr::copy_nonoverlapping(buf.as_ptr(), out, take * 2);
    }
    // Drop what was taken; anything beyond max stays for next drain.
    buf.drain(..take * 2);
    take
}

#[no_mangle]
pub extern "C" fn aw_recomp_audio_sample_rate(_h: *mut Host) -> u32 {
    // The APU produces interleaved stereo at 2x the nominal FIFO rate
    // (~1095 pairs per 59.727 Hz frame = 65536 Hz), matching what the
    // hardware mixer emits; the runtime's waveOut path adapts to it.
    65536
}

#[no_mangle]
pub extern "C" fn aw_recomp_memory_block(h: *mut Host, name: *const i8, size_out: *mut usize) -> *mut u8 {
    use std::ffi::CStr;
    if h.is_null() || name.is_null() {
        if !size_out.is_null() {
            unsafe { *size_out = 0 };
        }
        return std::ptr::null_mut();
    }
    let name = unsafe { CStr::from_ptr(name) }.to_string_lossy();
    let bus = &mut unsafe { &mut *h }.machine.bus;
    let (ptr, len): (*mut u8, usize) = match name.as_ref() {
        "wram" | "ewram" => (bus.ewram.as_mut_ptr(), bus.ewram.len()),
        "iwram" => (bus.iwram.as_mut_ptr(), bus.iwram.len()),
        "oam" => (bus.oam.as_mut_ptr(), bus.oam.len()),
        "vram" => (bus.vram.as_mut_ptr(), bus.vram.len()),
        "io" => (bus.io.as_mut_ptr(), bus.io.len()),
        "rom" => (bus.rom.as_mut_ptr(), bus.rom.len()),
        _ => (std::ptr::null_mut(), 0),
    };
    if !size_out.is_null() {
        unsafe { *size_out = len };
    }
    ptr
}

#[no_mangle]
pub extern "C" fn aw_recomp_read8(h: *mut Host, addr: u32) -> u8 {
    unsafe { &mut *h }.machine.bus.read8(addr)
}
#[no_mangle]
pub extern "C" fn aw_recomp_read16(h: *mut Host, addr: u32) -> u16 {
    unsafe { &mut *h }.machine.bus.read16(addr)
}
#[no_mangle]
pub extern "C" fn aw_recomp_write8(h: *mut Host, addr: u32, value: u8) {
    unsafe { &mut *h }.machine.bus.write8(addr, value)
}
#[no_mangle]
pub extern "C" fn aw_recomp_write16(h: *mut Host, addr: u32, value: u16) {
    unsafe { &mut *h }.machine.bus.write16(addr, value)
}

// --- Savestates (memory snapshots; the rewind/undo/F5-critical path) ---

#[no_mangle]
pub extern "C" fn aw_recomp_capture_snapshot(h: *mut Host) -> *mut Machine {
    if h.is_null() {
        return std::ptr::null_mut();
    }
    let mut snap = unsafe { &*h }.machine.clone();
    // Slim the snapshot: the ROM (4 MB, re-injected-free since restore
    // keeps the host's) and BIOS are the heavy immutables. The framebuffer
    // stays - the PPU indexes it in place after restore.
    snap.bus.rom = Vec::new();
    snap.bus.bios = Vec::new();
    Box::into_raw(Box::new(snap))
}

#[no_mangle]
pub extern "C" fn aw_recomp_restore_snapshot(h: *mut Host, snap: *mut Machine) -> i32 {
    if h.is_null() || snap.is_null() {
        return 0;
    }
    unsafe {
        let host = &mut *h;
        let snapshot = &*snap; // Borrowed, NOT consumed: the caller owns the
        // snapshot and frees it via aw_recomp_free_snapshot (the adapter
        // contract queries size after restore - mGBA's VFiles behaved the
        // same way).
        // In-place restore: every Vec allocation the host machine owns is
        // reused, so raw memory pointers cached by the embedder stay valid.
        host.machine.restore_from(snapshot);
        // The snapshot's ROM/BIOS are empty (slimmed at capture); neither is
        // restored in place - ROM is untouched, the PPU rewrites the
        // framebuffer next frame.
    }
    1
}

#[no_mangle]
pub extern "C" fn aw_recomp_free_snapshot(snap: *mut Machine) {
    if !snap.is_null() {
        unsafe { drop(Box::from_raw(snap)) };
    }
}

#[no_mangle]
pub extern "C" fn aw_recomp_snapshot_size(snap: *mut Machine) -> u64 {
    if snap.is_null() {
        return 0;
    }
    let m = unsafe { &*snap };
    let b = &m.bus;
    (b.ewram.len() + b.iwram.len() + b.vram.len() + b.palette.len() + b.oam.len()
        + b.io.len() + b.rom.len() + b.sram.len() + b.framebuffer.len() * 2) as u64
}

#[no_mangle]
pub extern "C" fn aw_recomp_save_state_file(h: *mut Host, path: *const i8) -> i32 {
    use std::ffi::CStr;
    if h.is_null() || path.is_null() {
        return 0;
    }
    let path_str = unsafe { CStr::from_ptr(path) }.to_string_lossy();
    let host = unsafe { &*h };
    match host.machine.save_state_to_file(&path_str) {
        Ok(()) => 1,
        Err(e) => {
            eprintln!("aw-recomp-host: failed to save state to {path_str}: {e}");
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn aw_recomp_load_state_file(h: *mut Host, path: *const i8) -> i32 {
    use std::ffi::CStr;
    if h.is_null() || path.is_null() {
        return 0;
    }
    let path_str = unsafe { CStr::from_ptr(path) }.to_string_lossy();
    let host = unsafe { &mut *h };
    match host.machine.load_state_from_file(&path_str) {
        Ok(()) => 1,
        Err(e) => {
            eprintln!("aw-recomp-host: failed to load state from {path_str}: {e}");
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn aw_recomp_reset(h: *mut Host) {
    if h.is_null() {
        return;
    }
    let h = unsafe { &mut *h };
    let rom = h.rom.clone();
    let video = h.video;
    h.machine = Machine::new(rom);
    h.video = video;
}
