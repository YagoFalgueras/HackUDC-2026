#include "receiver.h"
#include "doom_palette.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SAT_SCREEN_WIDTH   320
#define SAT_SCREEN_HEIGHT  200
#define SAT_FRAME_BYTES    (SAT_SCREEN_WIDTH * SAT_SCREEN_HEIGHT)

static int udp_socket = -1;

static unsigned char recv_buffer[SAT_FRAME_BYTES];
static unsigned char rgb_buffer[SAT_FRAME_BYTES * 3];

int receiver_init(int listen_port)
{
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
    {
        fprintf(stderr, "receiver_init: error creando socket UDP: %s\n", strerror(errno));
        return -1;
    }

    int optval = 1;
    setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "receiver_init: error en bind :%d: %s\n",
                listen_port, strerror(errno));
        close(udp_socket);
        udp_socket = -1;
        return -1;
    }

    fprintf(stderr, "receiver_init: escuchando en puerto %d\n", listen_port);
    return 0;
}

const uint8_t *receiver_poll(void)
{
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    ssize_t bytes = recvfrom(udp_socket, recv_buffer, sizeof(recv_buffer),
                             MSG_DONTWAIT,
                             (struct sockaddr *)&sender, &sender_len);

    if (bytes == SAT_FRAME_BYTES)
    {
        for (int i = 0; i < SAT_FRAME_BYTES; i++)
        {
            unsigned char idx = recv_buffer[i];
            rgb_buffer[i * 3 + 0] = doom_palette[idx][0];
            rgb_buffer[i * 3 + 1] = doom_palette[idx][1];
            rgb_buffer[i * 3 + 2] = doom_palette[idx][2];
        }
        return rgb_buffer;
    }

    if (bytes > 0)
        fprintf(stderr, "receiver_poll: frame parcial: %zd bytes (esperados %d)\n",
                bytes, SAT_FRAME_BYTES);
    else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        fprintf(stderr, "receiver_poll: error en recvfrom: %s\n", strerror(errno));

    return NULL;
}

void receiver_shutdown(void)
{
    if (udp_socket >= 0)
    {
        close(udp_socket);
        udp_socket = -1;
    }
}
