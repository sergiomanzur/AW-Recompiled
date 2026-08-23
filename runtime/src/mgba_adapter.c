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
#include <mgba-util/audio-buffer.h>
#include <mgba-util/image.h>
#include <mgba-util/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mCore* aw_mgba_create(const char* rom_path, void* video_buffer, size_t stride) {
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