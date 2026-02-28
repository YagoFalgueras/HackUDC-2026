#include "receiver.h"
#include "doom_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/* FFmpeg libs for H.264 decoding */
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

/* ---------- constantes ---------- */

#define SCREEN_WIDTH   176
#define SCREEN_HEIGHT  144
#define FRAME_BYTES    (SCREEN_WIDTH * SCREEN_HEIGHT)       /* 176*144 = 25344 pixels */
#define RGB_BUF_SIZE   (SCREEN_WIDTH * SCREEN_HEIGHT * 3)   /* RGB output: 176*144*3 = 75888 bytes */

#define UDP_BUF_SIZE   25344

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

/* Buffer de salida RGB */
static uint8_t *rgb_buffer = NULL;

/* H.264 reassembly buffer for FU-A fragments */
static uint8_t *h264_buf = NULL;
static int      h264_len = 0;

/* Buffer temporal para plano Y escalado (cuando resolución no coincide) */
static uint8_t *gray_buf = NULL;

/* FFmpeg decoder state */
static AVCodec *av_codec = NULL;
static AVCodecContext *av_ctx = NULL;
static AVFrame *av_frame = NULL;
static AVPacket *av_pkt = NULL;
static struct SwsContext *sws_ctx = NULL;

/* Contador de bytes recibidos (thread-safe) */
static _Atomic uint64_t g_bytes_received = 0;

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
 * Aplica doom_palette sobre el frame indexado y escribe en rgb_buffer.
 * Convierte datos de 8-bit indexed (1 byte/pixel) a RGB888 (3 bytes/pixel).
 * El plano Y del H.264 decodificado contiene los índices de paleta originales.
 */
static void apply_palette(const uint8_t *indexed, int len)
{
    for (int i = 0; i < len; i++)
    {
        uint8_t idx = indexed[i];
        rgb_buffer[i * 3 + 0] = doom_palette[idx][0];
        rgb_buffer[i * 3 + 1] = doom_palette[idx][1];
        rgb_buffer[i * 3 + 2] = doom_palette[idx][2];
    }
}

/**
 * Extrae el plano Y del frame decodificado y aplica la paleta DOOM.
 * Si la resolución coincide con SCREEN_WIDTH×SCREEN_HEIGHT, usa el plano Y
 * directamente. Si no, escala a GRAY8 primero.
 */
static int decode_frame_apply_palette(void)
{
    if (av_frame->width == SCREEN_WIDTH && av_frame->height == SCREEN_HEIGHT)
    {
        /* Plano Y directo: cada valor Y ≈ índice de paleta DOOM */
        const uint8_t *y_plane = av_frame->data[0];
        int y_stride = av_frame->linesize[0];

        if (y_stride == SCREEN_WIDTH) {
            /* Sin padding, copiar directamente */
            apply_palette(y_plane, FRAME_BYTES);
        } else {
            /* Con padding en cada línea, copiar fila a fila al buffer temporal */
            for (int row = 0; row < SCREEN_HEIGHT; row++)
                memcpy(gray_buf + row * SCREEN_WIDTH,
                       y_plane + row * y_stride, SCREEN_WIDTH);
            apply_palette(gray_buf, FRAME_BYTES);
        }
        return 1;
    }

    /* Resolución diferente: escalar plano Y a SCREEN_WIDTH×SCREEN_HEIGHT */
    if (!sws_ctx) {
        sws_ctx = sws_getContext(av_frame->width, av_frame->height,
                                 AV_PIX_FMT_GRAY8,
                                 SCREEN_WIDTH, SCREEN_HEIGHT,
                                 AV_PIX_FMT_GRAY8,
                                 SWS_BILINEAR, NULL, NULL, NULL);
    }
    if (sws_ctx) {
        uint8_t *dest[4] = { gray_buf, NULL, NULL, NULL };
        int dest_linesize[4] = { SCREEN_WIDTH, 0, 0, 0 };
        sws_scale(sws_ctx, (const uint8_t * const*)av_frame->data,
                  av_frame->linesize, 0, av_frame->height,
                  dest, dest_linesize);
        apply_palette(gray_buf, FRAME_BYTES);
        return 1;
    }

    return 0;
}

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
    rgb_buffer = malloc(RGB_BUF_SIZE);
    h264_buf   = malloc(200000); /* ample for NAL assembly */
    gray_buf   = malloc(FRAME_BYTES); /* buffer temporal para plano Y */
    if (!rgb_buffer || !h264_buf || !gray_buf)
    {
        fprintf(stderr, "receiver_init: malloc falló\n");
        receiver_shutdown();
        return -1;
    }

    /* Inicializar FFmpeg decoder for H.264 */
    av_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!av_codec) {
        fprintf(stderr, "receiver_init: avcodec_find_decoder failed\n");
        receiver_shutdown();
        return -1;
    }

    av_ctx = avcodec_alloc_context3(av_codec);
    if (!av_ctx) {
        fprintf(stderr, "receiver_init: avcodec_alloc_context3 failed\n");
        receiver_shutdown();
        return -1;
    }

    if (avcodec_open2(av_ctx, av_codec, NULL) < 0) {
        fprintf(stderr, "receiver_init: avcodec_open2 failed\n");
        avcodec_free_context(&av_ctx);
        av_ctx = NULL;
        receiver_shutdown();
        return -1;
    }

    av_frame = av_frame_alloc();
    av_pkt = av_packet_alloc();

    fprintf(stderr, "receiver_init: escuchando en puerto %d  "
            "(%dx%d H.264 + paleta DOOM, %d pixels por frame)\n",
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

        /* Incrementar contador de bytes recibidos */
        atomic_fetch_add(&g_bytes_received, (uint64_t)bytes);

        printf("UDP packet received: %zd bytes\n", bytes);

        /* Parsear header RTP */
        rtp_header_t rtp;
        int payload_off = unpack_rtp_header(udp_buf, (int)bytes, &rtp);
        if (payload_off < 0 || rtp.version != 2)
            continue;

        const uint8_t *payload     = udp_buf + payload_off;
        int            payload_len = (int)bytes - payload_off;
        if (payload_len <= 0)
            continue;

        /* Decodificar payload H.264 */

        if (payload_len >= 4 && payload[0] == 0x00 && payload[1] == 0x00 &&
            payload[2] == 0x00 && payload[3] == 0x01)
        {
            /* AnnexB: send directly to decoder */
            av_packet_unref(av_pkt);
            if (av_new_packet(av_pkt, payload_len) == 0) {
                memcpy(av_pkt->data, payload, payload_len);
                av_pkt->size = payload_len;
                if (avcodec_send_packet(av_ctx, av_pkt) == 0) {
                    while (avcodec_receive_frame(av_ctx, av_frame) == 0) {
                        if (decode_frame_apply_palette())
                            got_frame = 1;
                    }
                }
            }
        }
        else
        {
            uint8_t nal_type = payload[0] & 0x1F;
            if (nal_type == 28 && payload_len >= 2)
            {
                /* FU-A fragmentation */
                uint8_t fu_indicator = payload[0];
                uint8_t fu_header = payload[1];
                int S = (fu_header & 0x80) != 0;
                int E = (fu_header & 0x40) != 0;
                uint8_t orig_nal = (fu_indicator & 0xE0) | (fu_header & 0x1F);

                if (S) {
                    h264_len = 0;
                    /* start code */
                    uint8_t startcode[4] = {0,0,0,1};
                    memcpy(h264_buf + h264_len, startcode, 4);
                    h264_len += 4;
                    h264_buf[h264_len++] = orig_nal;
                    memcpy(h264_buf + h264_len, payload + 2, payload_len - 2);
                    h264_len += payload_len - 2;
                } else {
                    memcpy(h264_buf + h264_len, payload + 2, payload_len - 2);
                    h264_len += payload_len - 2;
                }

                if (E) {
                    /* complete NAL unit in h264_buf, decode it */
                    av_packet_unref(av_pkt);
                    if (av_new_packet(av_pkt, h264_len) == 0) {
                        memcpy(av_pkt->data, h264_buf, h264_len);
                        av_pkt->size = h264_len;
                        if (avcodec_send_packet(av_ctx, av_pkt) == 0) {
                            while (avcodec_receive_frame(av_ctx, av_frame) == 0) {
                                if (decode_frame_apply_palette())
                                    got_frame = 1;
                            }
                        }
                    }
                    h264_len = 0;
                }
            }
            else if (nal_type >= 1 && nal_type <= 23)
            {
                /* Single NAL unit (not fragmented) */
                av_packet_unref(av_pkt);
                /* prepend start code for safety */
                if (av_new_packet(av_pkt, payload_len + 4) == 0) {
                    av_pkt->data[0] = 0; av_pkt->data[1] = 0; av_pkt->data[2] = 0; av_pkt->data[3] = 1;
                    memcpy(av_pkt->data + 4, payload, payload_len);
                    av_pkt->size = payload_len + 4;
                    if (avcodec_send_packet(av_ctx, av_pkt) == 0) {
                        while (avcodec_receive_frame(av_ctx, av_frame) == 0) {
                            if (decode_frame_apply_palette())
                                got_frame = 1;
                        }
                    }
                }
            }
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

    free(rgb_buffer);
    rgb_buffer = NULL;

    if (h264_buf) {
        free(h264_buf);
        h264_buf = NULL;
    }

    if (gray_buf) {
        free(gray_buf);
        gray_buf = NULL;
    }

    /* Free FFmpeg resources if allocated */
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
    if (av_frame) {
        av_frame_free(&av_frame);
        av_frame = NULL;
    }
    if (av_pkt) {
        av_packet_free(&av_pkt);
        av_pkt = NULL;
    }
    if (av_ctx) {
        avcodec_free_context(&av_ctx);
        av_ctx = NULL;
    }
}

uint64_t receiver_get_bytes_received(void)
{
    return atomic_load(&g_bytes_received);
}
