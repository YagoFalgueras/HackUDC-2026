#include "downlink.h"

#include <stdio.h>
#include <string.h>

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
#endif

#define DEFAULT_DEST_IP   "127.0.0.1"
#define DEFAULT_DEST_PORT 9666

static int udp_socket = -1;
static struct sockaddr_in udp_dest;

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

    int sent = (int)sendto(udp_socket, buffer, size, 0,
                           (struct sockaddr *)&udp_dest, sizeof(udp_dest));

    printf("[SATELLITE] Sent UDP packet: %d bytes (requested: %zu)\n", sent, size);

    return sent;
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
