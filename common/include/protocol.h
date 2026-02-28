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

#ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <time.h>

/* TODO: Definir rtp_header_t para el downlink de vídeo */
/* TODO: Declarar funciones de serialización/deserialización de vídeo */

/* ------------------------------------------------------------------ */
/* Input packet (estación terrestre → satélite)                        */
/* ------------------------------------------------------------------ */

/* Bits del bitfield de teclas */
#define INPUT_BIT_FORWARD   (1 << 0)   /* Flecha arriba              */
#define INPUT_BIT_BACKWARD  (1 << 1)   /* Flecha abajo               */
#define INPUT_BIT_LEFT      (1 << 2)   /* Flecha izquierda (girar)   */
#define INPUT_BIT_RIGHT     (1 << 3)   /* Flecha derecha  (girar)    */
#define INPUT_BIT_FIRE      (1 << 4)   /* Ctrl — disparar            */
#define INPUT_BIT_USE       (1 << 5)   /* Espacio — usar/abrir       */
#define INPUT_BIT_SLEFT     (1 << 6)   /* , — strafe izquierda       */
#define INPUT_BIT_SRIGHT    (1 << 7)   /* . — strafe derecha         */
#define INPUT_BIT_RUN       (1 << 8)   /* Shift — correr             */
#define INPUT_BIT_STRAFE    (1 << 9)   /* Alt — modo strafe          */
#define INPUT_WEAPON_SHIFT  10         /* Bits 10-12: número de arma (0-6) */

typedef struct {
    uint16_t bitfield;    /* Teclas activas (bit flags) */
    uint32_t timestamp;   /* ms monótonos desde inicio del proceso */
    uint16_t seq_number;  /* Contador de paquetes enviados */
} input_packet_t;

/* Deserializa 8 bytes (big-endian) a input_packet_t */
static inline void unpack_input(const uint8_t *buf, input_packet_t *pkt)
{
    pkt->bitfield   = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    pkt->timestamp  = ((uint32_t)buf[2] << 24) | ((uint32_t)buf[3] << 16)
                    | ((uint32_t)buf[4] <<  8) |  (uint32_t)buf[5];
    pkt->seq_number = ((uint16_t)buf[6] << 8) | (uint16_t)buf[7];
}

/* Serializa input_packet_t a 8 bytes en orden big-endian */
static inline void pack_input(const input_packet_t *pkt, uint8_t *buf)
{
    buf[0] = (uint8_t)((pkt->bitfield  >> 8) & 0xFF);
    buf[1] = (uint8_t)( pkt->bitfield        & 0xFF);
    buf[2] = (uint8_t)((pkt->timestamp >> 24) & 0xFF);
    buf[3] = (uint8_t)((pkt->timestamp >> 16) & 0xFF);
    buf[4] = (uint8_t)((pkt->timestamp >>  8) & 0xFF);
    buf[5] = (uint8_t)( pkt->timestamp        & 0xFF);
    buf[6] = (uint8_t)((pkt->seq_number >> 8) & 0xFF);
    buf[7] = (uint8_t)( pkt->seq_number       & 0xFF);
}

/* Retorna milisegundos monótonos desde el inicio del proceso */
static inline uint32_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

#endif /* DOOM_SAT_PROTOCOL_H */
