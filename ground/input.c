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

static int               g_sock         = -1;
static struct sockaddr_in g_dest;
static uint16_t          g_bitfield      = 0;
static uint16_t          g_bitfield_prev = 0xFFFF; /* forzar envío inicial */
static uint16_t          g_seq           = 0;

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

    g_bitfield      = 0;
    g_bitfield_prev = 0xFFFF; /* garantiza envío del primer paquete */
    g_seq           = 0;

    fprintf(stderr, "input_init: listo, enviando a %s:%d\n",
            satellite_ip, satellite_port);
    return 0;
}

int input_poll(void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
            return 1;

        if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP)
        {
            int        pressed = (ev.type == SDL_KEYDOWN);
            SDL_Keycode key    = ev.key.keysym.sym;

            /* Salida de la aplicación */
            if (pressed && key == SDLK_ESCAPE)
                return 1;

            /* Movimiento y acciones */
            switch (key)
            {
                case SDLK_UP:
                    if (pressed) g_bitfield |=  INPUT_BIT_FORWARD;
                    else         g_bitfield &= ~INPUT_BIT_FORWARD;
                    break;

                case SDLK_DOWN:
                    if (pressed) g_bitfield |=  INPUT_BIT_BACKWARD;
                    else         g_bitfield &= ~INPUT_BIT_BACKWARD;
                    break;

                case SDLK_LEFT:
                    if (pressed) g_bitfield |=  INPUT_BIT_LEFT;
                    else         g_bitfield &= ~INPUT_BIT_LEFT;
                    break;

                case SDLK_RIGHT:
                    if (pressed) g_bitfield |=  INPUT_BIT_RIGHT;
                    else         g_bitfield &= ~INPUT_BIT_RIGHT;
                    break;

                case SDLK_LCTRL:
                case SDLK_RCTRL:
                    if (pressed) g_bitfield |=  INPUT_BIT_FIRE;
                    else         g_bitfield &= ~INPUT_BIT_FIRE;
                    break;

                case SDLK_SPACE:
                    if (pressed) g_bitfield |=  INPUT_BIT_USE;
                    else         g_bitfield &= ~INPUT_BIT_USE;
                    break;

                case SDLK_LSHIFT:
                    if (pressed) g_bitfield |=  INPUT_BIT_SLEFT;
                    else         g_bitfield &= ~INPUT_BIT_SLEFT;
                    break;

                case SDLK_RSHIFT:
                    if (pressed) g_bitfield |=  INPUT_BIT_SRIGHT;
                    else         g_bitfield &= ~INPUT_BIT_SRIGHT;
                    break;

                /* Selección de arma: teclas 1-7 */
                case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                case SDLK_5: case SDLK_6: case SDLK_7:
                    if (pressed)
                    {
                        uint16_t weapon = (uint16_t)(key - SDLK_1) & 0x7u;
                        g_bitfield = (uint16_t)(
                            (g_bitfield & ~(uint16_t)(0x7u << INPUT_WEAPON_SHIFT))
                            | (uint16_t)(weapon << INPUT_WEAPON_SHIFT)
                        );
                    }
                    else
                    {
                        /* Soltar la tecla de arma limpia el selector */
                        g_bitfield &= ~(uint16_t)(0x7u << INPUT_WEAPON_SHIFT);
                    }
                    break;

                default:
                    break;
            }
        }
    }

    /* Enviar solo si el estado cambió */
    if (g_bitfield != g_bitfield_prev)
    {
        input_packet_t pkt;
        uint8_t        buf[8];

        pkt.bitfield   = g_bitfield;
        pkt.timestamp  = get_timestamp_ms();
        pkt.seq_number = g_seq;

        pack_input(&pkt, buf);

        sendto(g_sock, (const char *)buf, sizeof(buf), 0,
               (const struct sockaddr *)&g_dest, (socklen_t)sizeof(g_dest));

        g_bitfield_prev = g_bitfield;
        g_seq++;
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
