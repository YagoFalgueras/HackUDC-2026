/*
 * main.c — Entry point de la estación terrestre
 *
 * Recibe el framebuffer crudo del satélite por UDP y lo renderiza con SDL2.
 * El satélite envía I_VideoBuffer: 320x200 píxeles, 8-bit paletted (64000 bytes).
 * Como no tenemos la paleta del satélite, renderizamos en escala de grises
 * mapeando cada índice de paleta directamente a luminancia.
 *
 * Parámetros por defecto:
 *   - IP escucha:  0.0.0.0  (todas las interfaces)
 *   - Puerto:      9666
 */

/* --- Includes estándar --- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

/* --- Includes de red (POSIX) --- */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

/* --- SDL2 --- */
#include <SDL2/SDL.h>

/* --- Paleta DOOM hardcodeada --- */
#include "doom_palette.h"

/* --- Constantes del framebuffer del satélite --- */
#define SAT_SCREEN_WIDTH   320
#define SAT_SCREEN_HEIGHT  200
#define SAT_FRAME_BYTES    (SAT_SCREEN_WIDTH * SAT_SCREEN_HEIGHT)  /* 64000 */

/* --- Parámetros de red por defecto --- */
#define DEFAULT_LISTEN_IP   "127.0.0.1"
#define DEFAULT_LISTEN_PORT 9666

/* --- Escala de ventana --- */
#define WINDOW_SCALE 3  /* 320x200 * 3 = 960x600 */

/* --- Estado global --- */
static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/*
 * init_udp_socket — Crea y bindea el socket UDP de recepción.
 * Retorna el fd del socket o -1 en error.
 */
static int init_udp_socket(const char *listen_ip, int port)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        fprintf(stderr, "Error creando socket UDP: %s\n", strerror(errno));
        return -1;
    }

    /* Permitir reutilizar el puerto rápidamente */
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, listen_ip, &addr.sin_addr);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "Error en bind %s:%d: %s\n",
                listen_ip, port, strerror(errno));
        close(sockfd);
        return -1;
    }

    fprintf(stderr, "Socket UDP escuchando en %s:%d\n", listen_ip, port);
    return sockfd;
}



int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* --- Captura de señales para shutdown limpio --- */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* --- Inicializar SDL2 --- */
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL_Init falló: %s\n", SDL_GetError());
        return 1;
    }

    /* --- Crear ventana --- */
    SDL_Window *window = SDL_CreateWindow(
        "UAC Orbital Relay - Ground Control",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SAT_SCREEN_WIDTH * WINDOW_SCALE,
        SAT_SCREEN_HEIGHT * WINDOW_SCALE,
        SDL_WINDOW_SHOWN
    );
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow falló: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* --- Crear renderer --- */
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer falló: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /*
     * Textura de streaming en formato RGB24.
     * Convertimos los 8-bit paletted a escala de grises (1 byte → 3 bytes RGB).
     */
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        SAT_SCREEN_WIDTH, SAT_SCREEN_HEIGHT
    );
    if (!texture)
    {
        fprintf(stderr, "SDL_CreateTexture falló: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* --- Inicializar socket UDP --- */
    int udp_socket = init_udp_socket(DEFAULT_LISTEN_IP, DEFAULT_LISTEN_PORT);
    if (udp_socket < 0)
    {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* --- Búferes de recepción y conversión --- */
    unsigned char recv_buffer[SAT_FRAME_BYTES];
    unsigned char rgb_buffer[SAT_FRAME_BYTES * 3];  /* 320x200x3 = 192000 */

    fprintf(stderr, "Esperando frames del satélite (%dx%d, %d bytes/frame)...\n",
            SAT_SCREEN_WIDTH, SAT_SCREEN_HEIGHT, SAT_FRAME_BYTES);

    /* --- Pintar negro inicial para que la ventana no quede sin contenido --- */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    /* --- Loop principal --- */
    while (g_running)
    {
        /* Procesar eventos SDL (cierre de ventana, teclado) */
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                g_running = 0;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                g_running = 0;
            }
        }

        /* Recibir frame por UDP (no bloqueante con timeout corto) */
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);

        ssize_t bytes = recvfrom(udp_socket, recv_buffer, sizeof(recv_buffer),
                                 MSG_DONTWAIT,
                                 (struct sockaddr *)&sender, &sender_len);

        if (bytes > 0 && bytes != SAT_FRAME_BYTES)
        {
            fprintf(stderr, "Frame parcial: %zd bytes (esperados %d)\n",
                    bytes, SAT_FRAME_BYTES);
        }

        if (bytes == SAT_FRAME_BYTES)
        {
            /*
             * Convertir 8-bit indexado → RGB24 usando la paleta DOOM.
             * Cada byte del framebuffer es un índice de paleta (0-255).
             */
            for (int i = 0; i < SAT_FRAME_BYTES; i++)
            {
                unsigned char idx = recv_buffer[i];
                rgb_buffer[i * 3 + 0] = doom_palette[idx][0];  /* R */
                rgb_buffer[i * 3 + 1] = doom_palette[idx][1];  /* G */
                rgb_buffer[i * 3 + 2] = doom_palette[idx][2];  /* B */
            }

            /* Volcar al GPU */
            SDL_UpdateTexture(texture, NULL, rgb_buffer,
                              SAT_SCREEN_WIDTH * 3);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
        else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            fprintf(stderr, "Error en recvfrom: %s\n", strerror(errno));
        }
        else if (bytes < 0)
        {
            /* Sin datos disponibles, pequeña pausa para no quemar CPU */
            SDL_Delay(1);
        }
    }

    /* --- Cleanup --- */
    fprintf(stderr, "\nCerrando ground station...\n");
    close(udp_socket);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
