/*
 * recomp_adapter.c — the gba-recomp static-recompilation backend.
 *
 * Implements the exact aw_mgba_* contract (see aw/mgba_adapter.h) by
 * forwarding to aw_recomp_host.dll, the Rust shim that embeds the
 * recompiled game DLL and its hardware runtime. Linking this file instead
 * of mgba_adapter.c switches the whole engine - probes, rewind, undo,
 * replays, sidebar - to native execution with zero changes elsewhere.
 *
 * Locating the pieces (first hit wins):
 *   host DLL:  $AW_RECOMP_HOST, else aw_recomp_host.dll next to the exe/CWD
 *   game DLL:  $AW_RECOMP_LIB, else ../gba-recomp/out/<rom-stem>.dll,
 *              else out/<rom-stem>.dll
 */
#ifdef AW_BACKEND_RECOMP

#include "aw/mgba_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// --- Host DLL bindings ----------------------------------------------------

struct RecompHostApi {
    void* (*create)(const char* rom_path, const char* dll_path, void* video);
    void (*destroy)(void* h);
    void (*run_frame)(void* h, unsigned short keys);
    size_t (*read_audio)(void* h, short* out, size_t max_pairs);
    unsigned (*audio_sample_rate)(void* h);
    void* (*memory_block)(void* h, const char* name, size_t* size_out);
    unsigned char (*read8)(void* h, unsigned addr);
    void (*write8)(void* h, unsigned addr, unsigned char value);
    unsigned short (*read16)(void* h, unsigned addr);
    void (*write16)(void* h, unsigned addr, unsigned short value);
    void* (*capture_snapshot)(void* h);
    int (*restore_snapshot)(void* h, void* snapshot);
    void (*free_snapshot)(void* snapshot);
    unsigned long long (*snapshot_size)(void* snapshot);
    int (*save_state_file)(void* h, const char* path);
    int (*load_state_file)(void* h, const char* path);
    void (*reset)(void* h);
};

static struct RecompHostApi g_host;
static int g_host_loaded = 0;

#ifdef _WIN32
static void* open_lib(const char* path) { return (void*)LoadLibraryA(path); }
static void* sym(void* lib, const char* name) { return (void*)GetProcAddress((HMODULE)lib, name); }
#else
static void* open_lib(const char* path) { return dlopen(path, RTLD_NOW); }
static void* sym(void* lib, const char* name) { return dlsym(lib, name); }
#endif

static int bind_host(void) {
    if (g_host_loaded) return g_host_loaded == 2;
    g_host_loaded = 1;

    const char* host_env = getenv("AW_RECOMP_HOST");
    void* lib = NULL;
    if (host_env != NULL && host_env[0] != '\0') {
        lib = open_lib(host_env);
    }
    if (lib == NULL) lib = open_lib("aw_recomp_host.dll");

    if (lib == NULL) {
        fprintf(stderr, "recomp backend: aw_recomp_host.dll not found "
                        "(build recomp-host/ or set AW_RECOMP_HOST)\n");
        return 0;
    }

    struct { const char* name; void** target; } binds[] = {
        {"aw_recomp_create", (void**)&g_host.create},
        {"aw_recomp_destroy", (void**)&g_host.destroy},
        {"aw_recomp_run_frame", (void**)&g_host.run_frame},
        {"aw_recomp_read_audio", (void**)&g_host.read_audio},
        {"aw_recomp_audio_sample_rate", (void**)&g_host.audio_sample_rate},
        {"aw_recomp_memory_block", (void**)&g_host.memory_block},
        {"aw_recomp_read8", (void**)&g_host.read8},
        {"aw_recomp_write8", (void**)&g_host.write8},
        {"aw_recomp_read16", (void**)&g_host.read16},
        {"aw_recomp_write16", (void**)&g_host.write16},
        {"aw_recomp_capture_snapshot", (void**)&g_host.capture_snapshot},
        {"aw_recomp_restore_snapshot", (void**)&g_host.restore_snapshot},
        {"aw_recomp_free_snapshot", (void**)&g_host.free_snapshot},
        {"aw_recomp_snapshot_size", (void**)&g_host.snapshot_size},
        {"aw_recomp_save_state_file", (void**)&g_host.save_state_file},
        {"aw_recomp_load_state_file", (void**)&g_host.load_state_file},
        {"aw_recomp_reset", (void**)&g_host.reset},
    };
    for (size_t i = 0; i < sizeof(binds) / sizeof(binds[0]); ++i) {
        *binds[i].target = sym(lib, binds[i].name);
        if (*binds[i].target == NULL) {
            fprintf(stderr, "recomp backend: host DLL missing %s\n", binds[i].name);
            return 0;
        }
    }
    g_host_loaded = 2;
    return 1;
}

// Find the recompiled game DLL for a ROM path. Searched in order:
//   $AW_RECOMP_LIB (full path), cwd-relative ../gba-recomp/out and out/,
//   then exe-relative walks (so double-clicking and any working
//   directory still find the sibling gba-recomp checkout).
static int find_game_dll(const char* rom_path, char* out, size_t out_size) {
    const char* env = getenv("AW_RECOMP_LIB");
    if (env != NULL && env[0] != '\0') {
        snprintf(out, out_size, "%s", env);
        return 1;
    }

    const char* slash = strrchr(rom_path, '\\');
    const char* slash2 = strrchr(rom_path, '/');
    const char* stem_start = rom_path;
    if (slash2 != NULL && slash2 > slash) slash = slash2;
    if (slash != NULL) stem_start = slash + 1;
    char stem[256];
    snprintf(stem, sizeof(stem), "%s", stem_start);
    char* dot = strrchr(stem, '.');
    if (dot != NULL) *dot = '\0';

    char candidates[8][600];
    int n = 0;
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (len > 0 && len < sizeof(exe_path)) {
        char* exe_dir_end = strrchr(exe_path, '\\');
        if (exe_dir_end != NULL) {
            *exe_dir_end = '\0';
            char exe_dir[MAX_PATH];
            snprintf(exe_dir, sizeof(exe_dir), "%s", exe_path);
            // build/<backend>/runtime -> repo root -> sibling checkout
            // (four levels: runtime, backend, build, repo root).
            snprintf(candidates[n++], sizeof(candidates[n]), "%s/../../../../gba-recomp/out/%s.dll", exe_dir, stem);
            snprintf(candidates[n++], sizeof(candidates[n]), "%s/out/%s.dll", exe_dir, stem);
            snprintf(candidates[n++], sizeof(candidates[n]), "%s/../gba-recomp/out/%s.dll", exe_dir, stem);
        }
    }
#endif
    snprintf(candidates[n++], sizeof(candidates[n]), "../gba-recomp/out/%s.dll", stem);
    snprintf(candidates[n++], sizeof(candidates[n]), "out/%s.dll", stem);

    for (int i = 0; i < n; ++i) {
        FILE* f = fopen(candidates[i], "rb");
        if (f != NULL) {
            fclose(f);
            snprintf(out, out_size, "%s", candidates[i]);
            return 1;
        }
    }
    fprintf(stderr, "recomp backend: no recompiled game DLL for '%s' "
                    "(run: recomp build <rom>; see data/recomp/README.md)\n",
            rom_path);
    return 0;
}

// --- aw_mgba_* contract ---------------------------------------------------

struct mCore* aw_mgba_create(const char* rom_path, void* video_buffer, size_t stride) {
    (void)stride;
    if (!bind_host()) return NULL;

    char dll_path[512];
    if (!find_game_dll(rom_path, dll_path, sizeof(dll_path))) return NULL;

    void* handle = g_host.create(rom_path, dll_path, video_buffer);
    return (struct mCore*)handle;
}

void aw_mgba_set_video_buffer(struct mCore* core, void* buffer, size_t stride) {
    (void)core; (void)buffer; (void)stride;
}

void aw_mgba_run_frame(struct mCore* core, uint16_t keys) {
    if (core == NULL) return;
    g_host.run_frame(core, keys);
}

void aw_mgba_reset(struct mCore* core) {
    if (core == NULL) return;
    g_host.reset(core);
}

size_t aw_mgba_read_audio(struct mCore* core, int16_t* samples, size_t max_samples) {
    if (core == NULL || samples == NULL || max_samples == 0) return 0;
    return g_host.read_audio(core, samples, max_samples);
}

unsigned aw_mgba_audio_sample_rate(const struct mCore* core) {
    if (core == NULL) return 0;
    return g_host.audio_sample_rate((void*)core);
}

void aw_mgba_destroy(struct mCore* core) {
    if (core == NULL) return;
    g_host.destroy(core);
}

uint8_t aw_mgba_read8(struct mCore* core, uint32_t address) {
    if (core == NULL) return 0;
    return g_host.read8(core, address);
}

void aw_mgba_write8(struct mCore* core, uint32_t address, uint8_t value) {
    if (core == NULL) return;
    g_host.write8(core, address, value);
}

uint16_t aw_mgba_read16(struct mCore* core, uint32_t address) {
    if (core == NULL) return 0;
    return g_host.read16(core, address);
}

void aw_mgba_write16(struct mCore* core, uint32_t address, uint16_t value) {
    if (core == NULL) return;
    g_host.write16(core, address, value);
}

void* aw_mgba_memory_block(struct mCore* core, const char* internal_name, size_t* size_out) {
    if (size_out != NULL) *size_out = 0;
    if (core == NULL || internal_name == NULL) return NULL;
    return g_host.memory_block(core, internal_name, size_out);
}

int aw_mgba_save_state(struct mCore* core, const char* path) {
    if (core == NULL || path == NULL) return 0;
    return g_host.save_state_file(core, path);
}

int aw_mgba_load_state(struct mCore* core, const char* path) {
    if (core == NULL || path == NULL) return 0;
    return g_host.load_state_file(core, path);
}

void* aw_mgba_capture_snapshot(struct mCore* core) {
    if (core == NULL) return NULL;
    return g_host.capture_snapshot(core);
}

int aw_mgba_restore_snapshot(struct mCore* core, void* snapshot) {
    if (core == NULL || snapshot == NULL) return 0;
    return g_host.restore_snapshot(core, snapshot);
}

size_t aw_mgba_snapshot_size(void* snapshot) {
    if (snapshot == NULL) return 0;
    return (size_t)g_host.snapshot_size(snapshot);
}

void aw_mgba_free_snapshot(void* snapshot) {
    if (snapshot == NULL) return;
    g_host.free_snapshot(snapshot);
}

#endif  // AW_BACKEND_RECOMP
