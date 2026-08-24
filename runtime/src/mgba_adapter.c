/*
 * mgba_adapter.c — Pure C bridge to libmgba.
 *
 * CRITICAL: <mgba/flags.h> MUST be included FIRST so that the struct layout
 * (MINIMAL_CORE, ENABLE_VFS, ENABLE_DIRECTORIES, etc.) matches mgba.lib exactly.
 * Without this, conditional struct fields shift every function pointer offset
 * and cause NULL-dereference crashes.
 */
#include <mgba/flags.h>

#include "aw/mgba_adapter.h"

#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/image.h>
#include <mgba-util/vfs.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// mGBA's default logger prints DEBUG lines (BIOS SWI/DMA traces) three times
// per frame to stdout. Formatting those dominates fast-forward and clutters
// every diagnostic log, so WARN-and-above go to stderr instead. Set
// AW_NATIVE_MGBA_LOGS=1 to restore the original chatty logger.
static void aw_quiet_log(struct mLogger* logger, int category, enum mLogLevel level,
                         const char* format, va_list args) {
    if (!(level & (mLOG_FATAL | mLOG_ERROR | mLOG_WARN))) return;

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    fprintf(stderr, "[mgba] %s\n", buffer);
    (void) logger;
    (void) category;
}

static struct mLogger aw_quiet_logger = {
    .log = aw_quiet_log,
};

static void aw_install_logger(void) {
    const char* verbose = getenv("AW_NATIVE_MGBA_LOGS");
    if (verbose != NULL && strcmp(verbose, "1") == 0) return;
    mLogSetDefaultLogger(&aw_quiet_logger);
}

struct mCore* aw_mgba_create(const char* rom_path, void* video_buffer, size_t stride) {
    aw_install_logger();

    struct mCore* core = mCoreFind(rom_path);
    if (!core) {
        fprintf(stderr, "[mgba] mCoreFind returned NULL for %s\n", rom_path);
        return NULL;
    }

    if (!core->init(core)) {
        fprintf(stderr, "[mgba] core->init failed\n");
        return NULL;
    }

    if (video_buffer && core->setVideoBuffer) {
        core->setVideoBuffer(core, (mColor*)video_buffer, stride);
    }

    if (!mCoreLoadFile(core, rom_path)) {
        fprintf(stderr, "[mgba] mCoreLoadFile failed\n");
        core->deinit(core);
        return NULL;
    }

    mCoreConfigInit(&core->config, NULL);
    mCoreConfigLoad(&core->config);

    struct mCoreOptions opts = {0};
    mCoreConfigMap(&core->config, &opts);
    opts.audioSync = false;
    opts.videoSync = false;
    opts.volume = 256;
    opts.mute = false;
    opts.sampleRate = 32768;
    opts.audioBuffers = 2048;
    mCoreConfigLoadDefaults(&core->config, &opts);
    mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "detect");
    mCoreLoadConfig(core);

    // Allocate an audio buffer large enough for a few frames of samples so
    // the frontend can drain it once per frame without dropping audio.
    if (core->setAudioBufferSize) {
        core->setAudioBufferSize(core, 2048);
    }

    core->reset(core);

    return core;
}

void aw_mgba_set_video_buffer(struct mCore* core, void* buffer, size_t stride) {
    if (!core) return;
    if (!core->setVideoBuffer) {
        fprintf(stderr, "[mgba] setVideoBuffer is NULL\n");
        return;
    }
    core->setVideoBuffer(core, (mColor*)buffer, stride);
}

void aw_mgba_run_frame(struct mCore* core, uint16_t keys) {
    if (!core) return;
    if (core->setKeys) {
        core->setKeys(core, keys);
    }
    if (core->runFrame) {
        core->runFrame(core);
    }
}

size_t aw_mgba_read_audio(struct mCore* core, int16_t* samples, size_t max_samples) {
    if (!core || !samples || max_samples == 0) return 0;
    if (!core->getAudioBuffer) return 0;

    struct mAudioBuffer* buffer = core->getAudioBuffer(core);
    if (!buffer) return 0;

    return mAudioBufferRead(buffer, samples, max_samples);
}

unsigned aw_mgba_audio_sample_rate(const struct mCore* core) {
    if (!core || !core->audioSampleRate) return 0;
    return core->audioSampleRate(core);
}

void aw_mgba_destroy(struct mCore* core) {
    if (core && core->deinit) {
        core->deinit(core);
    }
}

uint8_t aw_mgba_read8(struct mCore* core, uint32_t address) {
    if (!core || !core->rawRead8) return 0;
    return (uint8_t)core->rawRead8(core, address, -1);
}

void aw_mgba_write8(struct mCore* core, uint32_t address, uint8_t value) {
    if (!core || !core->rawWrite8) return;
    core->rawWrite8(core, address, -1, value);
}

uint16_t aw_mgba_read16(struct mCore* core, uint32_t address) {
    if (!core || !core->rawRead16) return 0;
    return (uint16_t)core->rawRead16(core, address, -1);
}

void aw_mgba_write16(struct mCore* core, uint32_t address, uint16_t value) {
    if (!core || !core->rawWrite16) return;
    core->rawWrite16(core, address, -1, value);
}

int aw_mgba_save_state(struct mCore* core, const char* path) {
    if (!core || !path) return 0;

    // mCoreSaveStateNamed memory-maps the file with MAP_WRITE. On this
    // project's Windows VFS backend (vfs-fd.c, selected by ENABLE_VFS_FD),
    // CreateFileMapping/MapViewOfFile for a writable mapping needs the
    // underlying file descriptor opened with read+write access -- O_WRONLY
    // alone makes the mapping (and therefore the whole save) silently fail.
    struct VFile* vf = VFileOpen(path, O_RDWR | O_CREAT | O_TRUNC);
    if (!vf) {
        fprintf(stderr, "[mgba] aw_mgba_save_state: VFileOpen failed for %s\n", path);
        return 0;
    }

    const bool ok = mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA);
    vf->close(vf);
    if (!ok) {
        fprintf(stderr, "[mgba] aw_mgba_save_state: mCoreSaveStateNamed failed for %s\n", path);
    }
    return ok ? 1 : 0;
}

int aw_mgba_load_state(struct mCore* core, const char* path) {
    if (!core || !path) return 0;

    struct VFile* vf = VFileOpen(path, O_RDONLY);
    if (!vf) {
        fprintf(stderr, "[mgba] aw_mgba_load_state: VFileOpen failed for %s\n", path);
        return 0;
    }

    const bool ok = mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA);
    vf->close(vf);
    if (!ok) {
        fprintf(stderr, "[mgba] aw_mgba_load_state: mCoreLoadStateNamed failed for %s\n", path);
    }
    return ok ? 1 : 0;
}

void* aw_mgba_capture_snapshot(struct mCore* core) {
    if (!core) return NULL;

    // VFileMemChunk starts empty and grows on write, so one call both
    // allocates and receives the savestate.
    struct VFile* vf = VFileMemChunk(NULL, 0);
    if (!vf) {
        fprintf(stderr, "[mgba] aw_mgba_capture_snapshot: VFileMemChunk failed\n");
        return NULL;
    }

    if (!mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA)) {
        fprintf(stderr, "[mgba] aw_mgba_capture_snapshot: mCoreSaveStateNamed failed\n");
        vf->close(vf);
        return NULL;
    }
    return vf;
}

int aw_mgba_restore_snapshot(struct mCore* core, void* snapshot) {
    if (!core || !snapshot) return 0;

    struct VFile* vf = (struct VFile*) snapshot;
    vf->seek(vf, 0, SEEK_SET);
    const bool ok = mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA);
    if (!ok) {
        fprintf(stderr, "[mgba] aw_mgba_restore_snapshot: mCoreLoadStateNamed failed\n");
    }
    return ok ? 1 : 0;
}

size_t aw_mgba_snapshot_size(void* snapshot) {
    if (!snapshot) return 0;
    struct VFile* vf = (struct VFile*) snapshot;
    const ssize_t size = vf->size(vf);
    return size > 0 ? (size_t) size : 0;
}

void aw_mgba_free_snapshot(void* snapshot) {
    if (!snapshot) return;
    struct VFile* vf = (struct VFile*) snapshot;
    vf->close(vf);
}

void* aw_mgba_memory_block(struct mCore* core, const char* internal_name, size_t* size_out) {
    if (size_out) *size_out = 0;
    if (!core || !internal_name) return NULL;
    if (!core->listMemoryBlocks || !core->getMemoryBlock) return NULL;

    const struct mCoreMemoryBlock* blocks = NULL;
    const size_t count = core->listMemoryBlocks(core, &blocks);
    if (!blocks) return NULL;

    for (size_t i = 0; i < count; ++i) {
        if (!blocks[i].internalName) continue;
        if (strcmp(blocks[i].internalName, internal_name) != 0) continue;

        size_t actual = 0;
        void* ptr = core->getMemoryBlock(core, blocks[i].id, &actual);
        if (!ptr) return NULL;
        if (size_out) *size_out = actual;
        return ptr;
    }
    return NULL;
}