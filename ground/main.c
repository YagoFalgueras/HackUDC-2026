// SPDX-FileCopyrightText: 2026 BFG-I contributors (HackUDC 2026)
// SPDX-License-Identifier: GPL-2.0-only
/*
 * main.c — Entry point de la estación terrestre
 *
 * Orquesta los módulos display, receiver e input.
 */

// IMPORTANTE: _POSIX_C_SOURCE debe definirse ANTES de cualquier include
// para habilitar CLOCK_MONOTONIC y clock_gettime
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "display.h"
#include "receiver.h"
#include "input.h"

// Puertos por defecto para compatibilidad con satellite/main.c
#define DEFAULT_SATELLITE_IP "127.0.0.1"
#define DEFAULT_VIDEO_PORT   5001  // Puerto para recibir vídeo del satélite
#define DEFAULT_INPUT_PORT   5000  // Puerto para enviar input al satélite

static volatile sig_atomic_t g_running = 1;
static struct timespec g_start_time;

static void signal_handler(int sig)
{
    (void)sig;
    printf("\n[GROUND] Señal %d recibida, iniciando apagado...\n", sig);

    // Calcular tiempo transcurrido
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_sec = (end_time.tv_sec - g_start_time.tv_sec) +
                         (end_time.tv_nsec - g_start_time.tv_nsec) / 1e9;

    // Obtener contadores de bytes
    uint64_t bytes_rx = receiver_get_bytes_received();
    uint64_t bytes_tx = input_get_bytes_sent();

    // Calcular ancho de banda en kbps
    double downlink_kbps = (elapsed_sec > 0) ? (bytes_rx * 8.0) / (elapsed_sec * 1000.0) : 0.0;
    double uplink_kbps = (elapsed_sec > 0) ? (bytes_tx * 8.0) / (elapsed_sec * 1000.0) : 0.0;

    // Reporte de estadísticas
    printf("\n=== REPORTE DE ANCHO DE BANDA (GROUND) ===\n");
    printf("Tiempo total: %.2f segundos\n", elapsed_sec);
    printf("DOWNLINK (video RX):\n");
    printf("  - Bytes recibidos: %lu\n", (unsigned long)bytes_rx);
    printf("  - Ancho de banda: %.2f kbps\n", downlink_kbps);
    printf("UPLINK (input TX):\n");
    printf("  - Bytes transmitidos: %lu\n", (unsigned long)bytes_tx);
    printf("  - Ancho de banda: %.2f kbps\n", uplink_kbps);
    printf("==========================================\n\n");

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

    // Capturar tiempo de inicio
    clock_gettime(CLOCK_MONOTONIC, &g_start_time);

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
