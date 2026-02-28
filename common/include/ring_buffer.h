/*
 * ring_buffer.h — Buffer circular lock-free para comunicación entre hilos
 *
 * Responsabilidad:
 *   Comunicación productor-consumidor sin bloqueos mutex en el path crítico.
 *   Usa operaciones atómicas en los índices de lectura/escritura.
 *
 * Estructuras a definir:
 *   - ring_buffer_t: Estructura opaca del buffer circular
 *     (puntero a memoria alineada, capacidad, tamaño de slot,
 *      índices atómicos de lectura y escritura)
 *
 * Funciones a declarar:
 *   - ring_buffer_create()   — Reserva memoria alineada para N slots de tamaño fijo
 *   - ring_buffer_destroy()  — Libera toda la memoria del buffer
 *   - ring_buffer_push()     — Escribe un elemento (retorna error si lleno). Usa atomic_store
 *   - ring_buffer_pop()      — Lee y consume el siguiente elemento. Usa atomic_load
 *   - ring_buffer_peek()     — Lee sin consumir
 *   - ring_buffer_count()    — Número de elementos actualmente en el buffer
 *   - ring_buffer_is_full()  — Comprueba si está lleno
 *   - ring_buffer_is_empty() — Comprueba si está vacío
 *   - ring_buffer_reset()    — Resetea índices a cero (no thread-safe, solo para reinicio)
 */

#ifndef DOOM_SAT_RING_BUFFER_H
#define DOOM_SAT_RING_BUFFER_H

/* TODO: Incluir stdatomic.h, stddef.h, stdbool.h */
/* TODO: Definir ring_buffer_t con campos atómicos */
/* TODO: Declarar funciones del ring buffer */

#endif /* DOOM_SAT_RING_BUFFER_H */
