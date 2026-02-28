// SPDX-FileCopyrightText: 2026 BFG-I contributors (HackUDC 2026)
// SPDX-License-Identifier: GPL-2.0-only
/*
 * ring_buffer.c — Implementación del buffer circular lock-free
 *
 * Implementar:
 *
 * ring_buffer_create(capacity, slot_size):
 *   - Reserva memoria alineada (posix_memalign o aligned_alloc) para capacity * slot_size
 *   - Inicializa índices atómicos de lectura y escritura a 0
 *   - Guarda capacity y slot_size en la estructura
 *
 * ring_buffer_destroy():
 *   - Libera la memoria del buffer con free()
 *
 * ring_buffer_push(rb, data):
 *   - Lee write_idx con atomic_load (memory_order_relaxed)
 *   - Calcula next_write = (write_idx + 1) % capacity
 *   - Si next_write == read_idx → buffer lleno, retorna error
 *   - Copia slot_size bytes de data al slot write_idx con memcpy
 *   - Actualiza write_idx con atomic_store (memory_order_release)
 *
 * ring_buffer_pop(rb, out):
 *   - Lee read_idx con atomic_load (memory_order_relaxed)
 *   - Si read_idx == write_idx → buffer vacío, retorna error
 *   - Copia slot_size bytes del slot read_idx a out con memcpy
 *   - Actualiza read_idx con atomic_store (memory_order_release)
 *
 * ring_buffer_peek(rb, out):
 *   - Como pop() pero sin avanzar read_idx
 *
 * ring_buffer_count():
 *   - Retorna (write_idx - read_idx + capacity) % capacity
 *
 * ring_buffer_is_full():
 *   - Retorna (write_idx + 1) % capacity == read_idx
 *
 * ring_buffer_is_empty():
 *   - Retorna write_idx == read_idx
 *
 * ring_buffer_reset():
 *   - Pone ambos índices a 0 (NO thread-safe, solo para reinicio controlado)
 */

#include "ring_buffer.h"

/* TODO: Implementar funciones */
