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
 * It converts the 8-bit paletted framebuffer to RGB format and writes it to the
 * next available ring buffer slot.
 *
 * @param pixels Pointer to DOOM's paletted framebuffer (8-bit indexed color)
 * @param width Frame width in pixels (typically 320 for DOOM)
 * @param height Frame height in pixels (typically 200 for DOOM)
 *
 * @return true if frame was written successfully, false if buffer was full
 *
 * @note This function performs palette lookup to convert 8-bit to 24-bit RGB
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

#endif // RINGBUFFER_H
