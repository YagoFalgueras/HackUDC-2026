/*
 * protocol.h — Definición del protocolo binario de comunicación satélite ↔ tierra
 *
 * Responsabilidad:
 *   Definir estructuras de paquetes y funciones de serialización/deserialización
 *   para la comunicación entre el satélite y la estación terrestre.
 *
 * Estructuras a definir:
 *   - video_packet_t: Header RTP simplificado + NAL unit H.264
 *     (sequence number, timestamp, marker bit para fin de frame, payload)
 *   - input_packet_t: Comando de input del jugador
 *     (bitfield de teclas presionadas, timestamp del cliente)
 *   - packet_header_t: Header común a todos los paquetes
 *     (versión del protocolo, tipo de paquete, longitud, checksum)
 *
 * Funciones a declarar:
 *   - protocol_pack_video_packet()   — Serializa un NAL unit H.264 en paquete RTP
 *   - protocol_unpack_video_packet() — Deserializa un paquete de vídeo recibido
 *   - protocol_pack_input_packet()   — Serializa un comando de input del jugador
 *   - protocol_unpack_input_packet() — Deserializa un paquete de input recibido
 *   - protocol_compute_checksum()    — Calcula CRC-16 o CRC-32 para detección de errores
 *   - protocol_validate_packet()     — Valida integridad (checksum, versión, longitud)
 */

#ifndef DOOM_SAT_PROTOCOL_H
#define DOOM_SAT_PROTOCOL_H

/* TODO: Incluir stdint.h, stddef.h */
/* TODO: Definir estructuras de paquetes */
/* TODO: Declarar funciones de serialización/deserialización */

#endif /* DOOM_SAT_PROTOCOL_H */
