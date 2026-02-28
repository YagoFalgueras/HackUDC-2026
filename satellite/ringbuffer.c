/**
 * @file ringbuffer.c
 * @brief Lock-free ring buffer implementation for DOOM frame transmission
 *
 * This module manages the shared memory ring buffer between DOOM rendering
 * thread and H.264 encoder thread.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "ringbuffer.h"
#include "protocol.h"

/**
 * Ring Buffer Structure
 *
 * - 3 slots for lock-free producer-consumer pattern
 * - Each slot contains RGB888 frame data + atomic status flag
 * - Status transitions: EMPTY → WRITING → READY → READING → EMPTY
 */

#define RING_BUFFER_SIZE 3

typedef struct {
    uint8_t frame_data[FRAME_WIDTH * FRAME_HEIGHT * 3]; // RGB888
    atomic_int status; // 0=EMPTY, 1=WRITING, 2=READY, 3=READING
    uint32_t frame_number;
} RingBufferSlot;

// Global ring buffer (shared between threads)
static RingBufferSlot g_ring_buffer[RING_BUFFER_SIZE];
static atomic_int g_write_index = ATOMIC_VAR_INIT(0);
static atomic_int g_read_index = ATOMIC_VAR_INIT(0);

// Frame counter for sequence tracking
static uint32_t g_frame_counter = 0;

// Current RGB palette (updated from DOOM's I_SetPalette)
static uint8_t g_palette_rgb[256][3];

/**
 * Initialize the ring buffer system
 */
void ringbuffer_init(void) {
    // Initialize all slots to EMPTY
    for (int i = 0; i < RING_BUFFER_SIZE; i++) {
        atomic_init(&g_ring_buffer[i].status, 0); // EMPTY
        g_ring_buffer[i].frame_number = 0;
    }

    atomic_init(&g_write_index, 0);
    atomic_init(&g_read_index, 0);
    g_frame_counter = 0;

    // Initialize palette with grayscale fallback
    for (int i = 0; i < 256; i++) {
        g_palette_rgb[i][0] = i; // R
        g_palette_rgb[i][1] = i; // G
        g_palette_rgb[i][2] = i; // B
    }

    printf("[RINGBUFFER] Initialized with %d slots\n", RING_BUFFER_SIZE);
}

/**
 * Shutdown the ring buffer system
 */
void ringbuffer_shutdown(void) {
    printf("[RINGBUFFER] Shutdown\n");
}

/**
 * Update the RGB palette
 *
 * This should be called from I_SetPalette() in i_video.c whenever
 * DOOM updates the color palette.
 *
 * @param palette_data Array of 256 RGB triplets
 */
void ringbuffer_update_palette(const uint8_t palette_data[256][3]) {
    memcpy(g_palette_rgb, palette_data, 256 * 3);
}

/**
 * Write a DOOM frame to the ring buffer
 *
 * Converts 320x200 8-bit paletted framebuffer to 176x144 RGB888
 * and writes it to the next available ring buffer slot.
 *
 * @param pixels Pointer to DOOM's paletted framebuffer (8-bit indexed)
 * @param width Frame width (should be 320)
 * @param height Frame height (should be 200)
 * @return true if frame written, false if buffer full (frame dropped)
 */
bool ringbuffer_write_frame(const uint8_t *pixels, int width, int height) {
    // Validate input parameters
    if (!pixels || width != 320 || height != 200) {
        fprintf(stderr, "[RINGBUFFER] Invalid frame dimensions (expected 320x200, got %dx%d)\n",
                width, height);
        return false;
    }

    // Get current write slot
    int slot = atomic_load(&g_write_index);
    int expected = 0; // EMPTY

    // Try to acquire slot using Compare-And-Swap (CAS)
    if (!atomic_compare_exchange_strong(&g_ring_buffer[slot].status, &expected, 1)) {
        // Slot busy - frame dropped
        // This is expected since DOOM runs at 35 Hz but encoder at 20 FPS
        return false;
    }

    // Status is now WRITING (1)
    // Convert 8-bit paletted → RGB888 with downsampling 320x200 → 176x144

    uint8_t *dest = g_ring_buffer[slot].frame_data;

    const float scale_x = (float)width / FRAME_WIDTH;
    const float scale_y = (float)height / FRAME_HEIGHT;

    for (int y = 0; y < FRAME_HEIGHT; y++) {
        for (int x = 0; x < FRAME_WIDTH; x++) {
            // Calculate source pixel (nearest neighbor sampling)
            int src_x = (int)(x * scale_x);
            int src_y = (int)(y * scale_y);
            int src_idx = src_y * width + src_x;

            // Get palette index from source
            uint8_t palette_idx = pixels[src_idx];

            // Convert to RGB using current palette
            int dest_idx = (y * FRAME_WIDTH + x) * 3;
            dest[dest_idx + 0] = g_palette_rgb[palette_idx][0]; // R
            dest[dest_idx + 1] = g_palette_rgb[palette_idx][1]; // G
            dest[dest_idx + 2] = g_palette_rgb[palette_idx][2]; // B
        }
    }

    // Update frame metadata
    g_ring_buffer[slot].frame_number = g_frame_counter++;

    // Mark as READY (2)
    atomic_store(&g_ring_buffer[slot].status, 2);

    // Advance write index (circular)
    atomic_store(&g_write_index, (slot + 1) % RING_BUFFER_SIZE);

    return true;
}

/**
 * Read a frame from the ring buffer (for encoder thread)
 *
 * This is an internal function that will be used by the encoder thread.
 * It's not exposed in ringbuffer.h since only the encoder should call it.
 *
 * @param frame_data Output buffer for RGB888 data
 * @param frame_number Output for frame sequence number
 * @return true if frame read, false if no frame available
 */
bool ringbuffer_read_frame(uint8_t *frame_data, uint32_t *frame_number) {
    int slot = atomic_load(&g_read_index);
    int expected = 2; // READY

    // Try to acquire ready frame
    if (!atomic_compare_exchange_strong(&g_ring_buffer[slot].status, &expected, 3)) {
        // No frame ready
        return false;
    }

    // Status is now READING (3)
    // Copy frame data
    memcpy(frame_data, g_ring_buffer[slot].frame_data,
           FRAME_WIDTH * FRAME_HEIGHT * 3);
    *frame_number = g_ring_buffer[slot].frame_number;

    // Mark as EMPTY (0)
    atomic_store(&g_ring_buffer[slot].status, 0);

    // Advance read index (circular)
    atomic_store(&g_read_index, (slot + 1) % RING_BUFFER_SIZE);

    return true;
}
