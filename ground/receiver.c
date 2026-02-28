#include "receiver.h"
#include "doom_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/* ---------- constantes ---------- */

#define SCREEN_WIDTH   176
#define SCREEN_HEIGHT  144
#define FRAME_BYTES    (SCREEN_WIDTH * SCREEN_HEIGHT * 3)   /* RGB raw: 176*144*3 = 75888 bytes */
#define RGB_BUF_SIZE   (FRAME_BYTES)

/* Máximo payload por paquete UDP (MTU 1500 - IP 20 - UDP 8 - RTP 12 = 1460) */
#define MAX_PAYLOAD    1460
#define UDP_BUF_SIZE   2048

/* ---------- RTP header (12 bytes fijos, RFC 3550) ---------- */

typedef struct {
    uint8_t  version;
    uint8_t  padding;
    uint8_t  extension;
    uint8_t  cc;
    uint8_t  marker;       /* 1 en el último fragmento del frame */
    uint8_t  payload_type;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_header_t;

/* ---------- estado del módulo ---------- */

static int udp_fd = -1;

/* Buffer de reensamblaje: acumula fragmentos hasta completar un frame */
static uint8_t *reassembly_buf = NULL;
static int      reassembly_len = 0;
static uint32_t reassembly_ts  = 0;   /* timestamp del frame en construcción */

/* Buffer de salida RGB */
static uint8_t *rgb_buffer = NULL;

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static int unpack_rtp_header(const uint8_t *buf, int len, rtp_header_t *hdr)
{
    if (len < 12)
        return -1;

    hdr->version      = (buf[0] >> 6) & 0x03;
    hdr->padding      = (buf[0] >> 5) & 0x01;
    hdr->extension    = (buf[0] >> 4) & 0x01;
    hdr->cc           = buf[0] & 0x0F;
    hdr->marker       = (buf[1] >> 7) & 0x01;
    hdr->payload_type = buf[1] & 0x7F;
    hdr->seq          = (uint16_t)(buf[2] << 8) | buf[3];
    hdr->timestamp    = (uint32_t)(buf[4] << 24) | (buf[5] << 16) |
                        (buf[6] << 8)  | buf[7];
    hdr->ssrc         = (uint32_t)(buf[8] << 24) | (buf[9] << 16) |
                        (buf[10] << 8) | buf[11];

    int payload_off = 12 + hdr->cc * 4;

    if (hdr->extension)
    {
        if (payload_off + 4 > len)
            return -1;
        uint16_t ext_len = (uint16_t)(buf[payload_off + 2] << 8) |
                           buf[payload_off + 3];
        payload_off += 4 + ext_len * 4;
    }

    return (payload_off <= len) ? payload_off : -1;
}

/**
 * COMENTADO: Aplica doom_palette sobre el frame indexado y escribe en rgb_buffer.
 * Ya no es necesario porque recibimos RGB raw directamente del satélite.
 */
// static void apply_palette(const uint8_t *indexed, int len)
// {
//     for (int i = 0; i < len; i++)
//     {
//         uint8_t idx = indexed[i];
//         rgb_buffer[i * 3 + 0] = doom_palette[idx][0];
//         rgb_buffer[i * 3 + 1] = doom_palette[idx][1];
//         rgb_buffer[i * 3 + 2] = doom_palette[idx][2];
//     }
// }

/* ------------------------------------------------------------------ */
/*  API pública                                                        */
/* ------------------------------------------------------------------ */

int receiver_init(int listen_port)
{
    /* Socket UDP */
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0)
    {
        fprintf(stderr, "receiver_init: socket(): %s\n", strerror(errno));
        return -1;
    }

    int optval = 1;
    setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    /* Non-blocking */
    int flags = fcntl(udp_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(udp_fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        fprintf(stderr, "receiver_init: fcntl O_NONBLOCK: %s\n", strerror(errno));
        close(udp_fd);
        udp_fd = -1;
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "receiver_init: bind(:%d): %s\n",
                listen_port, strerror(errno));
        close(udp_fd);
        udp_fd = -1;
        return -1;
    }

    /* Buffers */
    reassembly_buf = malloc(FRAME_BYTES);
    rgb_buffer     = malloc(RGB_BUF_SIZE);
    if (!reassembly_buf || !rgb_buffer)
    {
        fprintf(stderr, "receiver_init: malloc falló\n");
        receiver_shutdown();
        return -1;
    }
    reassembly_len = 0;
    reassembly_ts  = 0;

    fprintf(stderr, "receiver_init: escuchando en puerto %d  "
            "(%dx%d RGB raw, %d bytes por frame)\n",
            listen_port, SCREEN_WIDTH, SCREEN_HEIGHT, FRAME_BYTES);
    return 0;
}

const uint8_t *receiver_poll(void)
{
    uint8_t udp_buf[UDP_BUF_SIZE];
    int got_frame = 0;

    for (;;)
    {
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);

        ssize_t bytes = recvfrom(udp_fd, udp_buf, sizeof(udp_buf),
                                 MSG_DONTWAIT,
                                 (struct sockaddr *)&sender, &sender_len);

        if (bytes <= 0)
        {
            if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                fprintf(stderr, "receiver_poll: recvfrom: %s\n",
                        strerror(errno));
            break;
        }

        /* Parsear header RTP */
        rtp_header_t rtp;
        int payload_off = unpack_rtp_header(udp_buf, (int)bytes, &rtp);
        if (payload_off < 0 || rtp.version != 2)
            continue;

        const uint8_t *payload     = udp_buf + payload_off;
        int            payload_len = (int)bytes - payload_off;
        if (payload_len <= 0)
            continue;

        /* Si cambia el timestamp, empezar un frame nuevo */
        if (rtp.timestamp != reassembly_ts)
        {
            reassembly_len = 0;
            reassembly_ts  = rtp.timestamp;
        }

        /* Acumular payload en el buffer de reensamblaje */
        if (reassembly_len + payload_len <= FRAME_BYTES)
        {
            memcpy(reassembly_buf + reassembly_len, payload, payload_len);
            reassembly_len += payload_len;
        }
        else
        {
            fprintf(stderr, "receiver_poll: frame excede %d bytes, descartando\n",
                    FRAME_BYTES);
            reassembly_len = 0;
            continue;
        }

        /* Marker bit = último fragmento del frame */
        if (rtp.marker)
        {
            if (reassembly_len == FRAME_BYTES)
            {
                /* MODO RGB RAW: copiar directamente sin aplicar paleta */
                memcpy(rgb_buffer, reassembly_buf, FRAME_BYTES);
                got_frame = 1;

                /* MODO INDEXADO (comentado):
                // apply_palette(reassembly_buf, FRAME_BYTES / 3);
                // got_frame = 1;
                */
            }
            else
            {
                fprintf(stderr, "receiver_poll: frame incompleto: %d/%d bytes\n",
                        reassembly_len, FRAME_BYTES);
            }
            reassembly_len = 0;
        }
    }

    return got_frame ? rgb_buffer : NULL;
}

void receiver_shutdown(void)
{
    if (udp_fd >= 0)
    {
        close(udp_fd);
        udp_fd = -1;
    }

    free(reassembly_buf);
    reassembly_buf = NULL;
    free(rgb_buffer);
    rgb_buffer     = NULL;

    reassembly_len = 0;
    reassembly_ts  = 0;
}
