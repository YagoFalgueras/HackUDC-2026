#include "downlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#define DEFAULT_DEST_IP   "127.0.0.1"
#define DEFAULT_DEST_PORT 9666
#define MAX_PAYLOAD_SIZE  1460  // MTU 1500 - IP 20 - UDP 8 - RTP 12 = 1460 bytes payload
#define RTP_HEADER_SIZE   12
#define RTP_VERSION       2
#define RTP_PAYLOAD_TYPE  96    // Payload type para datos custom (96-127 son dinámicos)

// Estado RTP
static int udp_socket = -1;
static struct sockaddr_in udp_dest;
static uint16_t rtp_seq_number = 0;
static uint32_t rtp_ssrc = 0x12345678;  // Source identifier (puede ser random)
static uint32_t rtp_timestamp = 0;      // Timestamp incremental por frame

/**
 * pack_rtp_header - Construye un header RTP de 12 bytes
 * @buf: Buffer de salida (mínimo 12 bytes)
 * @seq: Número de secuencia
 * @timestamp: Timestamp del frame
 * @marker: Bit marker (1 si es último fragmento del frame)
 */
static void pack_rtp_header(uint8_t *buf, uint16_t seq, uint32_t timestamp, uint8_t marker)
{
    // Byte 0: V(2) | P(1) | X(1) | CC(4)
    buf[0] = (RTP_VERSION << 6) | 0x00;  // Version=2, sin padding, sin extensión, CC=0

    // Byte 1: M(1) | PT(7)
    buf[1] = ((marker & 0x01) << 7) | (RTP_PAYLOAD_TYPE & 0x7F);

    // Bytes 2-3: Sequence number (big-endian)
    buf[2] = (seq >> 8) & 0xFF;
    buf[3] = seq & 0xFF;

    // Bytes 4-7: Timestamp (big-endian)
    buf[4] = (timestamp >> 24) & 0xFF;
    buf[5] = (timestamp >> 16) & 0xFF;
    buf[6] = (timestamp >> 8) & 0xFF;
    buf[7] = timestamp & 0xFF;

    // Bytes 8-11: SSRC (big-endian)
    buf[8] = (rtp_ssrc >> 24) & 0xFF;
    buf[9] = (rtp_ssrc >> 16) & 0xFF;
    buf[10] = (rtp_ssrc >> 8) & 0xFF;
    buf[11] = rtp_ssrc & 0xFF;
}

int downlink_init(const char *dest_ip, uint16_t dest_port)
{
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
    {
        fprintf(stderr, "downlink_init: failed to create socket\n");
        return -1;
    }

    memset(&udp_dest, 0, sizeof(udp_dest));
    udp_dest.sin_family = AF_INET;
    udp_dest.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &udp_dest.sin_addr);

    fprintf(stderr, "downlink_init: streaming to %s:%d\n", dest_ip, dest_port);
    return 0;
}

int downlink_send_raw_frame(const void *buffer, size_t size)
{
    if (udp_socket < 0)
    {
        if (downlink_init(DEFAULT_DEST_IP, DEFAULT_DEST_PORT) < 0)
            return -1;
    }

    if (size == 0)
        return 0;

    const uint8_t *data = (const uint8_t *)buffer;
    size_t bytes_remaining = size;
    size_t bytes_sent_total = 0;
    int num_fragments = 0;

    // Incrementar timestamp para este frame
    rtp_timestamp++;

    // Fragmentar y enviar
    while (bytes_remaining > 0)
    {
        // Calcular tamaño del payload para este fragmento
        size_t payload_size = (bytes_remaining > MAX_PAYLOAD_SIZE)
                              ? MAX_PAYLOAD_SIZE
                              : bytes_remaining;

        // Determinar si es el último fragmento
        uint8_t is_last = (payload_size == bytes_remaining) ? 1 : 0;

        // Construir paquete RTP: header (12 bytes) + payload
        uint8_t packet[RTP_HEADER_SIZE + MAX_PAYLOAD_SIZE];

        // Empaquetar header RTP
        pack_rtp_header(packet, rtp_seq_number, rtp_timestamp, is_last);

        // Copiar payload
        memcpy(packet + RTP_HEADER_SIZE, data, payload_size);

        size_t packet_size = RTP_HEADER_SIZE + payload_size;

        // Enviar paquete
        ssize_t sent = sendto(udp_socket, packet, packet_size, 0,
                             (struct sockaddr *)&udp_dest, sizeof(udp_dest));

        if (sent < 0)
        {
            fprintf(stderr, "[SATELLITE ERROR] sendto() failed on fragment %d: %s\n",
                    num_fragments, strerror(errno));
            return -1;
        }

        if ((size_t)sent != packet_size)
        {
            fprintf(stderr, "[SATELLITE WARNING] Partial send: %zd/%zu bytes on fragment %d\n",
                    sent, packet_size, num_fragments);
        }

        // Avanzar punteros
        data += payload_size;
        bytes_remaining -= payload_size;
        bytes_sent_total += payload_size;
        num_fragments++;
        rtp_seq_number++;
    }

    printf("[SATELLITE] Sent frame: %zu bytes in %d RTP fragments (timestamp=%u)\n",
           bytes_sent_total, num_fragments, rtp_timestamp);

    return (int)bytes_sent_total;
}

int downlink_send_nals(uint8_t **nals, size_t *nal_sizes, int num_nals, uint32_t timestamp)
{
    (void)nals;
    (void)nal_sizes;
    (void)num_nals;
    (void)timestamp;
    /* TODO: implementar empaquetado RTP/H.264 */
    return -1;
}

void downlink_shutdown(void)
{
    if (udp_socket >= 0)
    {
#ifdef _WIN32
        closesocket(udp_socket);
#else
        close(udp_socket);
#endif
        udp_socket = -1;
    }
}
