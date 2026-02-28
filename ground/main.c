/*
 * main.c — Entry point de la estación terrestre
 *
 * Orquesta los módulos display, receiver e input.
 */

#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "display.h"
#include "receiver.h"
#include "input.h"

#define DEFAULT_LISTEN_PORT 9666

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    if (display_init() < 0)
        return 1;

    if (receiver_init(DEFAULT_LISTEN_PORT) < 0)
    {
        display_shutdown();
        return 1;
    }

    if (input_init("127.0.0.1", 9667) < 0)
    {
        receiver_shutdown();
        display_shutdown();
        return 1;
    }

    fprintf(stderr, "Esperando frames del satélite...\n");

    while (g_running)
    {
        if (input_poll())
            g_running = 0;

        const uint8_t *frame = receiver_poll();
        if (frame)
            display_present_frame(frame);
        else
            SDL_Delay(1);
    }

    fprintf(stderr, "\nCerrando ground station...\n");
    input_shutdown();
    receiver_shutdown();
    display_shutdown();

    return 0;
}
