/*
 * main.c — Test binary del satélite: envía frames de color sólido por downlink
 *
 * Responsabilidad:
 *   Verificar el pipeline de vídeo completo (satellite → ground) sin necesitar
 *   DOOM. Envía frames 320×200 8bpp a 20 FPS con índice de paleta creciente,
 *   produciendo colores cambiantes en la ventana SDL de la estación terrestre.
 */

#define _POSIX_C_SOURCE 200809L

#include "downlink.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>

#define FRAME_W     320
#define FRAME_H     200
#define FRAME_BYTES (FRAME_W * FRAME_H)
#define FPS         20

static volatile int g_running = 1;

static void on_signal(int s)
{
    (void)s;
    g_running = 0;
}

int main(void)
{
    uint8_t frame[FRAME_BYTES];
    uint8_t color = 0;
    struct timespec delay;

    delay.tv_sec  = 0;
    delay.tv_nsec = 1000000000L / FPS;

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    if (downlink_init("127.0.0.1", 9666) < 0)
        return 1;

    fprintf(stderr, "satellite test: enviando frames fake a %d FPS\n", FPS);

    while (g_running)
    {
        memset(frame, color, FRAME_BYTES);
        color++;

        downlink_send_raw_frame(frame, FRAME_BYTES);
        nanosleep(&delay, NULL);
    }

    fprintf(stderr, "satellite test: terminando\n");
    downlink_shutdown();
    return 0;
}
