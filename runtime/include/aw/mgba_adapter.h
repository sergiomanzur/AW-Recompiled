#ifndef AW_MGBA_ADAPTER_H
#define AW_MGBA_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mCore;

struct mCore* aw_mgba_create(const char* rom_path, void* video_buffer, size_t stride);
void aw_mgba_set_video_buffer(struct mCore* core, void* buffer, size_t stride);
void aw_mgba_run_frame(struct mCore* core, uint16_t keys);

// Reads up to `max_samples` stereo sample frames from the core's audio buffer
// into `samples` (interleaved stereo int16). Returns the number of sample
// frames actually read.
size_t aw_mgba_read_audio(struct mCore* core, int16_t* samples, size_t max_samples);

// Returns the core's audio sample rate in Hz (0 on failure).
unsigned aw_mgba_audio_sample_rate(const struct mCore* core);

void aw_mgba_destroy(struct mCore* core);

// Direct game memory access (for cursor position read/write)
uint8_t aw_mgba_read8(struct mCore* core, uint32_t address);
void aw_mgba_write8(struct mCore* core, uint32_t address, uint8_t value);
uint16_t aw_mgba_read16(struct mCore* core, uint32_t address);
void aw_mgba_write16(struct mCore* core, uint32_t address, uint16_t value);

// Returns a pointer to a live core memory block looked up by mGBA's internal
// name ("wram", "iwram", "oam", "io"), or NULL. Looking blocks up by name
// avoids depending on mGBA's internal region enum.
void* aw_mgba_memory_block(struct mCore* core, const char* internal_name, size_t* size_out);

// Savestate capture/restore for offline tooling (e.g. the cursor-coordinate
// miner) and the F5 capture hotkey. Returns 1 on success, 0 on failure; never
// crashes the game on failure.
int aw_mgba_save_state(struct mCore* core, const char* path);
int aw_mgba_load_state(struct mCore* core, const char* path);

#ifdef __cplusplus
}
#endif

#endif