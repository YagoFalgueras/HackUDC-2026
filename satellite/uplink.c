/*
 * uplink.c — Receptor UDP de input del jugador (uplink RX)
 *
 * Responsabilidad:
 *   Recibir paquetes de input enviados por la estación terrestre, deserializarlos,
 *   detectar cambios en el bitfield de teclas y encolar eventos key_event_t para
 *   que DG_GetKey() los consuma desde el game thread.
 */

#define _POSIX_C_SOURCE 200809L

#include "uplink.h"
#include "../common/include/protocol.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

/* Keycodes internos de DOOM (de doomkeys.h) */
#define DK_UPARROW    0xad
#define DK_DOWNARROW  0xaf
#define DK_LEFTARROW  0xac
#define DK_RIGHTARROW 0xae
#define DK_RCTRL      (0x80 + 0x1d)
#define DK_SPACE      ' '
#define DK_COMMA      ','
#define DK_PERIOD     '.'
#define DK_RSHIFT     (0x80 + 0x36)
#define DK_RALT       (0x80 + 0x38)

/* ------------------------------------------------------------------ */
/* Estado interno                                                       */
/* ------------------------------------------------------------------ */

#define KEY_QUEUE_SIZE 64

static int             g_sock          = -1;
static key_event_t     g_queue[KEY_QUEUE_SIZE];
static int             g_queue_head    = 0;
static int             g_queue_tail    = 0;
static pthread_mutex_t g_queue_mutex   = PTHREAD_MUTEX_INITIALIZER;
static uint16_t        g_prev_bitfield = 0;

/* ------------------------------------------------------------------ */
/* Helper: encolar un evento de tecla (thread-safe)                    */
/* ------------------------------------------------------------------ */

static void enqueue_key_event(uint16_t key, bool pressed)
{
    int next_tail;

    pthread_mutex_lock(&g_queue_mutex);

    next_tail = (g_queue_tail + 1) % KEY_QUEUE_SIZE;
    if (next_tail == g_queue_head)
    {
        /* Cola llena: descartar evento */
        pthread_mutex_unlock(&g_queue_mutex);
        return;
    }

    g_queue[g_queue_tail].key     = key;
    g_queue[g_queue_tail].pressed = pressed;
    g_queue_tail = next_tail;

    pthread_mutex_unlock(&g_queue_mutex);
}

/* ------------------------------------------------------------------ */
/* Helper: traducir un bit del bitfield a keycode DOOM y encolarlo     */
/* ------------------------------------------------------------------ */

static void process_bit(uint16_t cur, uint16_t prev, uint16_t mask,
                        uint16_t doom_key)
{
    int cur_set  = (cur  & mask) != 0;
    int prev_set = (prev & mask) != 0;

    if (cur_set && !prev_set)
        enqueue_key_event(doom_key, true);
    else if (!cur_set && prev_set)
        enqueue_key_event(doom_key, false);
}

/* ------------------------------------------------------------------ */
/* Helper: manejar los bits 10-12 de selección de arma                 */
/* ------------------------------------------------------------------ */

static void process_weapon(uint16_t cur, uint16_t prev)
{
    uint16_t cur_w  = (cur  >> INPUT_WEAPON_SHIFT) & 0x7u;
    uint16_t prev_w = (prev >> INPUT_WEAPON_SHIFT) & 0x7u;

    if (cur_w == prev_w)
        return;

    /* Soltar arma anterior */
    if (prev_w > 0)
        enqueue_key_event((uint16_t)('0' + prev_w), false);

    /* Pulsar arma nueva */
    if (cur_w > 0)
        enqueue_key_event((uint16_t)('0' + cur_w), true);
}

/* ------------------------------------------------------------------ */
/* Funciones públicas                                                   */
/* ------------------------------------------------------------------ */

int uplink_init(uint16_t listen_port)
{
    struct sockaddr_in addr;
    int flags;

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0)
    {
        fprintf(stderr, "uplink_init: socket() falló: %s\n", strerror(errno));
        return -1;
    }

    /* Non-blocking */
    flags = fcntl(g_sock, F_GETFL, 0);
    if (flags < 0 || fcntl(g_sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        fprintf(stderr, "uplink_init: fcntl() falló: %s\n", strerror(errno));
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_sock, (const struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "uplink_init: bind() en puerto %u falló: %s\n",
                (unsigned)listen_port, strerror(errno));
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    pthread_mutex_init(&g_queue_mutex, NULL);
    g_queue_head    = 0;
    g_queue_tail    = 0;
    g_prev_bitfield = 0;

    fprintf(stderr, "uplink_init: escuchando en UDP port %u\n",
            (unsigned)listen_port);
    return 0;
}

int uplink_poll(void)
{
    uint8_t        buf[8];
    ssize_t        n;
    int            count = 0;
    input_packet_t pkt;

    while (1)
    {
        n = recvfrom(g_sock, buf, sizeof(buf), 0, NULL, NULL);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;   /* No hay más paquetes */
            fprintf(stderr, "uplink_poll: recvfrom() error: %s\n",
                    strerror(errno));
            return -1;
        }
        if (n != 8)
            continue;    /* Paquete malformado, ignorar */

        unpack_input(buf, &pkt);

        /* Teclas de movimiento y acción */
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_FORWARD,  DK_UPARROW);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_BACKWARD, DK_DOWNARROW);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_LEFT,     DK_LEFTARROW);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_RIGHT,    DK_RIGHTARROW);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_FIRE,     DK_RCTRL);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_USE,    DK_SPACE);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_SLEFT,  DK_COMMA);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_SRIGHT, DK_PERIOD);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_RUN,    DK_RSHIFT);
        process_bit(pkt.bitfield, g_prev_bitfield, INPUT_BIT_STRAFE, DK_RALT);

        /* Selección de arma (bits 10-12) */
        process_weapon(pkt.bitfield, g_prev_bitfield);

        g_prev_bitfield = pkt.bitfield;
        count++;
    }

    return count;
}

int uplink_pop_key(uint16_t *key, bool *pressed)
{
    int ret = 0;

    pthread_mutex_lock(&g_queue_mutex);

    if (g_queue_head != g_queue_tail)
    {
        *key     = g_queue[g_queue_head].key;
        *pressed = g_queue[g_queue_head].pressed;
        g_queue_head = (g_queue_head + 1) % KEY_QUEUE_SIZE;
        ret = 1;
    }

    pthread_mutex_unlock(&g_queue_mutex);
    return ret;
}

void uplink_shutdown(void)
{
    if (g_sock >= 0)
    {
        close(g_sock);
        g_sock = -1;
    }
    pthread_mutex_destroy(&g_queue_mutex);
}
