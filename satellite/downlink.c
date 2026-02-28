#include "downlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>

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

// Contador de bytes transmitidos (thread-safe)
static _Atomic uint64_t g_bytes_sent = 0;

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


int downlink_send_nals(uint8_t **nals, size_t *nal_sizes, int num_nals, uint32_t timestamp)
{
    if (udp_socket < 0 || !nals || !nal_sizes || num_nals <= 0)
        return -1;

    int total_packets = 0;
    uint8_t packet[RTP_HEADER_SIZE + MAX_PAYLOAD_SIZE];

    for (int i = 0; i < num_nals; i++)
    {
        uint8_t *nal = nals[i];
        size_t nal_size = nal_sizes[i];

        if (nal_size == 0)
            continue;

        int is_last_nal = (i == num_nals - 1);

        if (nal_size <= MAX_PAYLOAD_SIZE)
        {
            // Single NAL unit packet: RTP header + NAL completo
            uint8_t marker = is_last_nal ? 1 : 0;
            pack_rtp_header(packet, rtp_seq_number, timestamp, marker);
            memcpy(packet + RTP_HEADER_SIZE, nal, nal_size);

            ssize_t sent = sendto(udp_socket, packet, RTP_HEADER_SIZE + nal_size, 0,
                                  (struct sockaddr *)&udp_dest, sizeof(udp_dest));
            if (sent < 0)
            {
                fprintf(stderr, "[DOWNLINK ERROR] sendto() failed: %s\n", strerror(errno));
                return -1;
            }
            // Incrementar contador de bytes enviados (incluye header RTP + payload)
            atomic_fetch_add(&g_bytes_sent, (uint64_t)sent);

            rtp_seq_number++;
            total_packets++;
        }
        else
        {
            // Fragmentación FU-A
            // FU indicator (1 byte): F | NRI del NAL original | Type=28 (FU-A)
            // FU header   (1 byte): S | E | R | Type del NAL original
            uint8_t nal_header = nal[0];
            uint8_t fu_indicator = (nal_header & 0xE0) | 28;  // NRI + FU-A type
            uint8_t nal_type = nal_header & 0x1F;

            // Payload del NAL sin el primer byte (ya codificado en FU indicator/header)
            uint8_t *nal_payload = nal + 1;
            size_t remaining = nal_size - 1;
            size_t max_frag_payload = MAX_PAYLOAD_SIZE - 2;  // -2 para FU indicator + FU header
            int frag_index = 0;

            while (remaining > 0)
            {
                size_t frag_size = (remaining > max_frag_payload)
                                   ? max_frag_payload : remaining;
                int is_first = (frag_index == 0);
                int is_last_frag = (frag_size == remaining);

                // Marker bit solo en el último fragmento del último NAL
                uint8_t marker = (is_last_nal && is_last_frag) ? 1 : 0;
                pack_rtp_header(packet, rtp_seq_number, timestamp, marker);

                // FU indicator
                packet[RTP_HEADER_SIZE] = fu_indicator;

                // FU header: S(1) | E(1) | R(1) | Type(5)
                packet[RTP_HEADER_SIZE + 1] = nal_type;
                if (is_first)
                    packet[RTP_HEADER_SIZE + 1] |= 0x80;  // S bit
                if (is_last_frag)
                    packet[RTP_HEADER_SIZE + 1] |= 0x40;  // E bit

                // Payload del fragmento
                memcpy(packet + RTP_HEADER_SIZE + 2, nal_payload, frag_size);

                size_t pkt_size = RTP_HEADER_SIZE + 2 + frag_size;
                ssize_t sent = sendto(udp_socket, packet, pkt_size, 0,
                                      (struct sockaddr *)&udp_dest, sizeof(udp_dest));
                if (sent < 0)
                {
                    fprintf(stderr, "[DOWNLINK ERROR] sendto() FU-A frag %d failed: %s\n",
                            frag_index, strerror(errno));
                    return -1;
                } else
                {
                    printf("[DOWNLINK] Sent FU-A fragment %d/%zu for NAL %d (size=%zu bytes)\n",
                           frag_index + 1, (remaining + max_frag_payload - 1) / max_frag_payload,
                           i, frag_size);
                }

                // Actualizar contador de bytes enviados
                atomic_fetch_add(&g_bytes_sent, (uint64_t)sent);

                nal_payload += frag_size;
                remaining -= frag_size;
                rtp_seq_number++;
                total_packets++;
                frag_index++;
            }
        }
    }

    return total_packets;
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

uint64_t downlink_get_bytes_sent(void)
{
    return atomic_load(&g_bytes_sent);
}
