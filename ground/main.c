/*
 * main.c — Entry point de la estación terrestre
 *
 * Orquesta los módulos display, receiver e input.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "display.h"
#include "receiver.h"
#include "input.h"

// Puertos por defecto para compatibilidad con satellite/main.c
#define DEFAULT_SATELLITE_IP "127.0.0.1"
#define DEFAULT_VIDEO_PORT   5001  // Puerto para recibir vídeo del satélite
#define DEFAULT_INPUT_PORT   5000  // Puerto para enviar input al satélite

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[])
{
    const char *satellite_ip = DEFAULT_SATELLITE_IP;
    int video_port = DEFAULT_VIDEO_PORT;  // Recibir vídeo
    int input_port = DEFAULT_INPUT_PORT;  // Enviar input

    // Parsear argumentos: ./doom-ground <SATELLITE_IP> <VIDEO_PORT> <INPUT_PORT>
    if (argc >= 4) {
        satellite_ip = argv[1];
        video_port = atoi(argv[2]);
        input_port = atoi(argv[3]);
        fprintf(stderr, "[GROUND] Configuración desde argumentos:\n");
        fprintf(stderr, "[GROUND]   - IP satélite: %s\n", satellite_ip);
        fprintf(stderr, "[GROUND]   - Puerto vídeo (recibir): %d\n", video_port);
        fprintf(stderr, "[GROUND]   - Puerto input (enviar): %d\n", input_port);
    } else if (argc > 1) {
        satellite_ip = argv[1];
        fprintf(stderr, "[GROUND] IP satélite: %s (desde argv)\n", satellite_ip);
        fprintf(stderr, "[GROUND] Puertos por defecto: vídeo=%d, input=%d\n", video_port, input_port);
    } else {
        fprintf(stderr, "[GROUND] Usando configuración por defecto:\n");
        fprintf(stderr, "[GROUND]   - IP satélite: %s\n", satellite_ip);
        fprintf(stderr, "[GROUND]   - Puerto vídeo (recibir): %d\n", video_port);
        fprintf(stderr, "[GROUND]   - Puerto input (enviar): %d\n", input_port);
        fprintf(stderr, "[GROUND] Uso: %s <SATELLITE_IP> <VIDEO_PORT> <INPUT_PORT>\n", argv[0]);
        fprintf(stderr, "[GROUND] Ejemplo: %s 192.168.1.100 5001 5000\n", argv[0]);
    }

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    if (display_init() < 0)
        return 1;

    if (receiver_init(video_port) < 0)
    {
        display_shutdown();
        return 1;
    }

    if (input_init(satellite_ip, input_port) < 0)
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
        
        if (frame){
            printf("[GROUND] Pasando un frame!");
            display_present_frame(frame);
        }      
        else
            SDL_Delay(1);
    }

    fprintf(stderr, "\nCerrando ground station...\n");
    input_shutdown();
    receiver_shutdown();
    display_shutdown();

    return 0;
}
