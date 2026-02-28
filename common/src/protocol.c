// SPDX-FileCopyrightText: 2026 BFG-I contributors (HackUDC 2026)
// SPDX-License-Identifier: GPL-2.0-only
/*
 * protocol.c — Serialización/deserialización del protocolo binario
 *
 * Implementar:
 *
 * protocol_pack_video_packet():
 *   - Recibe un NAL unit H.264 (buffer + tamaño) y metadatos RTP (seq number, timestamp, marker)
 *   - Escribe el header RTP simplificado seguido del payload NAL en un buffer de salida
 *   - Retorna el tamaño total del paquete serializado
 *
 * protocol_unpack_video_packet():
 *   - Recibe un buffer raw de la red
 *   - Parsea el header RTP, extrae sequence number, timestamp, marker bit
 *   - Retorna puntero al payload NAL unit y su tamaño
 *
 * protocol_pack_input_packet():
 *   - Recibe el bitfield de input del jugador y el timestamp del cliente
 *   - Serializa en un paquete binario compacto de tamaño fijo (INPUT_PACKET_SIZE)
 *
 * protocol_unpack_input_packet():
 *   - Deserializa un paquete de input recibido en el satélite
 *   - Extrae bitfield de teclas y timestamp
 *
 * protocol_compute_checksum():
 *   - Calcula CRC-16 o CRC-32 sobre el payload del paquete
 *   - Usado antes de enviar y al recibir para detectar corrupción
 *
 * protocol_validate_packet():
 *   - Verifica: checksum correcto, versión del protocolo soportada, longitud coherente
 *   - Retorna código de error específico si falla alguna validación
 */

#include "protocol.h"

/* TODO: Implementar funciones */
