/*
 * input.c — Captura de teclado SDL2 y transmisión por UDP (uplink)
 *
 * Responsabilidad:
 *   Leer eventos de teclado SDL2, mantener un bitfield de teclas activas y,
 *   cuando el estado cambia, serializar el paquete de input y enviarlo por
 *   UDP al satélite.
 */

#include "input.h"
#include "../common/include/protocol.h"

#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  define CLOSE_SOCK(s) closesocket(s)
   typedef int socklen_t;
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  define CLOSE_SOCK(s) close(s)
#endif

/* ------------------------------------------------------------------ */
/* Estado interno del módulo                                           */
/* ------------------------------------------------------------------ */

static int               g_sock          = -1;
static struct sockaddr_in g_dest;
static uint16_t          g_bitfield      = 0;
static uint16_t          g_bitfield_prev = 0;
static uint16_t          g_seq           = 0;

#define INPUT_SEND_INTERVAL_MS 50   /* 20 Hz = cada 50 ms */
static uint32_t          g_last_send_ms = 0;

static const char *bit_name(uint16_t bit)
{
    switch (bit)
    {
        case INPUT_BIT_FORWARD:  return "FORWARD";
        case INPUT_BIT_BACKWARD: return "BACKWARD";
        case INPUT_BIT_LEFT:     return "LEFT";
        case INPUT_BIT_RIGHT:    return "RIGHT";
        case INPUT_BIT_FIRE:     return "FIRE";
        case INPUT_BIT_USE:      return "USE";
        case INPUT_BIT_SLEFT:    return "SLEFT";
        case INPUT_BIT_SRIGHT:   return "SRIGHT";
        case INPUT_BIT_RUN:      return "RUN";
        case INPUT_BIT_STRAFE:   return "STRAFE";
        case INPUT_BIT_ENTER:    return "ENTER";
        case INPUT_BIT_ESCAPE:   return "ESCAPE";
        default:                 return "?";
    }
}

/* ------------------------------------------------------------------ */
/* Funciones públicas                                                  */
/* ------------------------------------------------------------------ */

int input_init(const char *satellite_ip, int satellite_port)
{
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0)
    {
        fprintf(stderr, "input_init: no se pudo crear el socket UDP\n");
        return -1;
    }

    memset(&g_dest, 0, sizeof(g_dest));
    g_dest.sin_family      = AF_INET;
    g_dest.sin_port        = htons((uint16_t)satellite_port);
    if (inet_pton(AF_INET, satellite_ip, &g_dest.sin_addr) != 1)
    {
        fprintf(stderr, "input_init: IP inválida: %s\n", satellite_ip);
        CLOSE_SOCK(g_sock);
        g_sock = -1;
        return -1;
    }

    g_bitfield = 0;
    g_seq      = 0;

    fprintf(stderr, "input_init: listo, enviando a %s:%d\n",
            satellite_ip, satellite_port);
    return 0;
}

int input_poll(void)
{
    SDL_Event ev;

    /* Solo procesar eventos para detectar cierre de ventana */
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
            return 1;
    }

    /* Fotografía instantánea del estado físico del teclado */
    {
        static const SDL_Scancode weapon_sc[7] = {
            SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
            SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7
        };
        const uint8_t *ks = SDL_GetKeyboardState(NULL);
        uint16_t       new_bf = 0;
        int            w;

        if (ks[SDL_SCANCODE_UP])    new_bf |= INPUT_BIT_FORWARD;
        if (ks[SDL_SCANCODE_DOWN])  new_bf |= INPUT_BIT_BACKWARD;
        if (ks[SDL_SCANCODE_LEFT])  new_bf |= INPUT_BIT_LEFT;
        if (ks[SDL_SCANCODE_RIGHT]) new_bf |= INPUT_BIT_RIGHT;

        if (ks[SDL_SCANCODE_LCTRL]  || ks[SDL_SCANCODE_RCTRL])  new_bf |= INPUT_BIT_FIRE;
        if (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]) new_bf |= INPUT_BIT_RUN;
        if (ks[SDL_SCANCODE_LALT]   || ks[SDL_SCANCODE_RALT])   new_bf |= INPUT_BIT_STRAFE;

        if (ks[SDL_SCANCODE_PERIOD]) new_bf |= INPUT_BIT_SRIGHT;
        if (ks[SDL_SCANCODE_COMMA])  new_bf |= INPUT_BIT_SLEFT;
        if (ks[SDL_SCANCODE_SPACE])  new_bf |= INPUT_BIT_USE;
        if (ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_KP_ENTER]) new_bf |= INPUT_BIT_ENTER;
        if (ks[SDL_SCANCODE_ESCAPE]) new_bf |= INPUT_BIT_ESCAPE;

        /* Selección de arma: primera tecla activa gana */
        for (w = 0; w < 7; w++)
        {
            if (ks[weapon_sc[w]])
            {
                new_bf |= (uint16_t)(((uint16_t)(w + 1) & 0x7u) << INPUT_WEAPON_SHIFT);
                break;
            }
        }

        g_bitfield = new_bf;
    }

    /* Debug: imprimir cambios de bitfield */
    if (g_bitfield != g_bitfield_prev)
    {
        uint16_t changed = g_bitfield ^ g_bitfield_prev;
        uint16_t bit;
        for (bit = 1; bit != 0 && bit <= 0x7FFF; bit <<= 1)
        {
            if (!(changed & bit))
                continue;
            if (bit & (uint16_t)(0x7u << INPUT_WEAPON_SHIFT))
                continue; /* weapon handled separately */
            fprintf(stderr, "[GROUND TX] key %-8s %s  (bitfield=0x%04x)\n",
                    bit_name(bit),
                    (g_bitfield & bit) ? "PRESSED " : "RELEASED",
                    g_bitfield);
        }
        /* Weapon bits */
        {
            uint16_t cur_w  = (g_bitfield      >> INPUT_WEAPON_SHIFT) & 0x7u;
            uint16_t prev_w = (g_bitfield_prev >> INPUT_WEAPON_SHIFT) & 0x7u;
            if (cur_w != prev_w)
            {
                if (prev_w) fprintf(stderr, "[GROUND TX] WEAPON %u RELEASED\n", prev_w);
                if (cur_w)  fprintf(stderr, "[GROUND TX] WEAPON %u PRESSED\n",  cur_w);
            }
        }
        g_bitfield_prev = g_bitfield;
    }

    /* Enviar estado a tasa fija de 20 Hz (redundancia contra pérdida de paquetes) */
    {
        uint32_t now = get_timestamp_ms();

        if (now - g_last_send_ms >= INPUT_SEND_INTERVAL_MS)
        {
            input_packet_t pkt;
            uint8_t        buf[8];

            pkt.bitfield   = g_bitfield;
            pkt.timestamp  = now;
            pkt.seq_number = g_seq;

            pack_input(&pkt, buf);

            sendto(g_sock, (const char *)buf, sizeof(buf), 0,
                   (const struct sockaddr *)&g_dest, (socklen_t)sizeof(g_dest));

            g_seq++;
            g_last_send_ms = now;
        }
    }

    return 0;
}

void input_shutdown(void)
{
    if (g_sock >= 0)
    {
        CLOSE_SOCK(g_sock);
        g_sock = -1;
    }
}
