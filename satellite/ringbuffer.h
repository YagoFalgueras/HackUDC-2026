// SPDX-FileCopyrightText: 2026 BFG-I contributors (HackUDC 2026)
// SPDX-License-Identifier: GPL-2.0-only
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file ringbuffer.h
 * @brief Thread-safe ring buffer interface for DOOM frame transmission
 *
 * This module provides a lock-free ring buffer for communication between
 * the DOOM rendering thread and the H.264 encoder thread.
 *
 * DOOM thread calls ringbuffer_write_frame() from I_FinishUpdate()
 * Encoder thread reads frames via the internal ring buffer mechanism
 *
 * Thread Safety:
 * - Lock-free implementation using atomic operations
 * - Single producer (DOOM thread), single consumer (encoder thread)
 * - Safe to call from different threads simultaneously
 */

/**
 * @brief Write a DOOM frame to the ring buffer
 *
 * This function is called from DOOM's I_FinishUpdate() to submit rendered frames.
 * It writes the 8-bit indexed framebuffer (grayscale) directly to the ring buffer
 * without RGB conversion, preserving bandwidth.
 *
 * @param pixels Pointer to DOOM's paletted framebuffer (8-bit indexed color)
 * @param width Frame width in pixels (typically 320 for DOOM)
 * @param height Frame height in pixels (typically 200 for DOOM)
 *
 * @return true if frame was written successfully, false if buffer was full
 *
 * @note This function downsamples and stores grayscale (1 byte/pixel)
 * @note Palette conversion is deferred to ground station for bandwidth optimization
 * @note Non-blocking: returns immediately if no slot is available
 * @note Thread-safe: uses atomic operations for state transitions
 *
 * Example usage from i_video.c:
 * @code
 * void I_FinishUpdate(void) {
 *     ringbuffer_write_frame(I_VideoBuffer, SCREENWIDTH, SCREENHEIGHT);
 * }
 * @endcode
 */
bool ringbuffer_write_frame(const uint8_t *pixels, int width, int height);

/**
 * @brief Initialize the ring buffer system
 *
 * Must be called before any ringbuffer_write_frame() calls.
 * Sets up the shared memory and atomic state variables.
 *
 * @note Called automatically from satellite/main.c during startup
 */
void ringbuffer_init(void);

/**
 * @brief Shutdown the ring buffer system
 *
 * Cleanup function for graceful termination.
 *
 * @note Called automatically from satellite/main.c during shutdown
 */
void ringbuffer_shutdown(void);

/**
 * @brief Update the RGB palette used for indexed→RGB conversion
 *
 * This function should be called from I_SetPalette() whenever DOOM updates
 * its color palette (on startup, powerups, damage, etc.)
 *
 * @param palette_data Array of 256 RGB triplets [256][3]
 *
 * @note Thread-safe: palette update is atomic
 * @note Must be called before frames are written to get correct colors
 */
void ringbuffer_update_palette(const uint8_t palette_data[256][3]);

/**
 * @brief Read a frame from the ring buffer
 *
 * This function is called from the encoder thread to retrieve rendered frames.
 * It attempts to read the next available frame from the ring buffer.
 *
 * @param frame_data Output buffer for grayscale data (must be at least FRAME_WIDTH * FRAME_HEIGHT bytes)
 * @param frame_number Output for frame sequence number
 *
 * @return true if frame was read successfully, false if no frame available
 *
 * @note Non-blocking: returns immediately if no frame is ready
 * @note Thread-safe: uses atomic operations for state transitions
 */
bool ringbuffer_read_frame(uint8_t *frame_data, uint32_t *frame_number);

#endif // RINGBUFFER_H
