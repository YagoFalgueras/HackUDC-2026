#ifndef UPLINK_H
#define UPLINK_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Estructura que representa un evento de tecla del jugador
 */
typedef struct {
    uint16_t key;      // Código de tecla (keycode interno de DOOM)
    bool pressed;      // true = tecla presionada, false = tecla liberada
} key_event_t;

/**
 * uplink_init - Inicializa el receptor de input por uplink
 * @listen_port: Puerto UDP local donde escuchar paquetes de input
 *
 * Crea el socket UDP de escucha configurado en modo non-blocking.
 * Vincula el socket al puerto especificado para recibir paquetes de input
 * desde la estación terrestre.
 *
 * Inicializa la cola de input thread-safe con su mutex para almacenar
 * eventos de teclas que serán consumidos por DG_GetKey() en el game thread.
 *
 * Returns: 0 en éxito, -1 en error
 */
int uplink_init(uint16_t listen_port);

/**
 * uplink_poll - Procesa paquetes de input pendientes
 *
 * Realiza recvfrom() en modo non-blocking para recibir paquetes de input
 * del jugador. Puede procesar múltiples paquetes en una sola llamada si
 * hay varios pendientes.
 *
 * Para cada paquete recibido:
 * 1. Deserializa el paquete usando unpack_input() de protocol.h
 * 2. Traduce el bitfield del protocolo a eventos de teclas de DOOM
 * 3. Encola los eventos en la cola de input thread-safe
 *
 * Detección de cambios:
 * - Compara el bitfield actual con el anterior
 * - Solo genera eventos para teclas que cambiaron de estado
 * - Genera evento pressed=true para nuevas teclas presionadas
 * - Genera evento pressed=false para teclas liberadas
 *
 * Esta función debe ser llamada regularmente desde el game thread
 * (ej: una vez por cada iteración del game loop).
 *
 * Returns: Número de paquetes procesados, 0 si no había ninguno, -1 en error
 */
int uplink_poll(void);

/**
 * uplink_pop_key - Desencola el siguiente evento de tecla
 * @key: Puntero donde se escribirá el código de tecla
 * @pressed: Puntero donde se escribirá el estado (presionada/liberada)
 *
 * Desencola el siguiente evento de tecla de la cola thread-safe.
 * Esta función es llamada por DG_GetKey() desde el game thread.
 *
 * Thread-safety: La cola está protegida por un mutex interno, por lo
 * que es seguro llamar esta función desde el game thread mientras
 * uplink_poll() la alimenta potencialmente desde otro contexto.
 *
 * Returns: 1 si se desencoló un evento (key y pressed contienen datos válidos),
 *          0 si la cola estaba vacía
 */
int uplink_pop_key(uint16_t *key, bool *pressed);

/**
 * uplink_shutdown - Finaliza el receptor y libera recursos
 *
 * Cierra el socket UDP, destruye el mutex de la cola de input y
 * libera cualquier otro recurso asociado.
 */
void uplink_shutdown(void);

#endif /* UPLINK_H */
