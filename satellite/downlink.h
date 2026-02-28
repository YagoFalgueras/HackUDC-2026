#ifndef DOWNLINK_H
#define DOWNLINK_H

#include <stdint.h>
#include <stddef.h>

/**
 * downlink_init - Inicializa el transmisor de vídeo por downlink
 * @dest_ip: Dirección IP de la estación terrestre (destino)
 * @dest_port: Puerto UDP de destino para vídeo
 *
 * Crea el socket UDP de salida y configura la estructura sockaddr_in
 * del destino (estación terrestre). Inicializa el sequence number RTP a 0.
 *
 * El socket se configura para envío UDP sin opciones especiales.
 *
 * Returns: 0 en éxito, -1 en error
 */
int downlink_init(const char *dest_ip, uint16_t dest_port);

/**
 * downlink_send_nals - Transmite NAL units H.264 empaquetados en RTP/UDP
 * @nals: Array de punteros a NAL units
 * @nal_sizes: Array de tamaños de cada NAL unit
 * @num_nals: Número de NAL units a transmitir
 * @timestamp: Timestamp RTP del frame (en unidades de 90 kHz típicamente)
 *
 * Para cada NAL unit:
 * - Si el tamaño del NAL d MTU: se envía en un único paquete RTP con
 *   header RTP estándar
 * - Si el tamaño del NAL > MTU: se fragmenta usando FU-A (Fragmentation
 *   Unit type A) en múltiples paquetes RTP
 *
 * El último paquete del frame se marca con marker bit = 1 en el header RTP.
 * El sequence number RTP se incrementa en cada paquete enviado.
 *
 * Fragmentación FU-A:
 * - Primer fragmento: FU indicator + FU header (con bit S=1) + payload
 * - Fragmentos intermedios: FU indicator + FU header + payload
 * - Último fragmento: FU indicator + FU header (con bit E=1) + payload
 *
 * Returns: Número de paquetes RTP enviados, o -1 en error
 */
int downlink_send_nals(uint8_t **nals, size_t *nal_sizes, int num_nals, uint32_t timestamp);

/**
 * downlink_shutdown - Finaliza el transmisor y libera recursos
 *
 * Cierra el socket UDP y libera cualquier recurso asociado.
 */
void downlink_shutdown(void);

#endif /* DOWNLINK_H */
