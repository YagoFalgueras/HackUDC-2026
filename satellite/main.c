/**
 * satellite/main.c - Entry point y orquestador del satélite
 *
 * VERSIÓN MINIMALISTA - Implementación paso a paso:
 *
 * Este archivo debe:
 * 1. Inicializar todos los subsistemas (encoder, uplink, downlink, DOOM)
 * 2. Crear y gestionar 2 hilos con afinidad de CPU
 * 3. Implementar ring buffer lock-free compartido entre hilos
 * 4. Gestionar ciclo de vida y cleanup al terminar
 */

// Source - https://stackoverflow.com/a/40515669
// Retrieved 2026-02-28, License - CC BY-SA 3.0
// IMPORTANTE: _POSIX_C_SOURCE debe definirse ANTES de cualquier include
// para habilitar CLOCK_MONOTONIC, clock_nanosleep y strdup
// Requiere POSIX.1-2008 (200809L) para strdup
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "protocol.h"
#include "encoder.h"
#include "downlink.h"
#include "uplink.h"
#include "ringbuffer.h"



/**
 * PASO 1: DEFINIR RING BUFFER
 *
 * El ring buffer permite comunicación lock-free entre el hilo de DOOM
 * y el hilo de encoding/transmisión.
 *
 * Características:
 * - Tamaño: 3-4 slots (balance entre latencia y pérdida de frames)
 * - Cada slot contiene: frame RGB (176×144×3 bytes) + flag atómico
 * - Flags posibles: EMPTY (0), WRITING (1), READY (2), READING (3)
 */

// Ring buffer ahora se maneja en ringbuffer.c/h
// (eliminado el ring buffer local para evitar duplicación)

/**
 * PASO 2: VARIABLES GLOBALES DE CONTROL
 *
 * Control de hilos y señales de terminación
 */

static volatile sig_atomic_t g_running = 1;
static pthread_t g_doom_thread;
static pthread_t g_encoder_thread;
static struct timespec g_start_time;

/**
 * PASO 3: HANDLER DE SEÑALES
 *
 * Captura Ctrl+C para apagado limpio
 */

void signal_handler(int signum) {
    printf("\n[SATELLITE] Señal %d recibida, iniciando apagado...\n", signum);

    // Calcular tiempo transcurrido
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_sec = (end_time.tv_sec - g_start_time.tv_sec) +
                         (end_time.tv_nsec - g_start_time.tv_nsec) / 1e9;

    // Obtener contadores de bytes
    uint64_t bytes_tx = downlink_get_bytes_sent();
    uint64_t bytes_rx = uplink_get_bytes_received();

    // Calcular ancho de banda en kbps
    double downlink_kbps = (elapsed_sec > 0) ? (bytes_tx * 8.0) / (elapsed_sec * 1000.0) : 0.0;
    double uplink_kbps = (elapsed_sec > 0) ? (bytes_rx * 8.0) / (elapsed_sec * 1000.0) : 0.0;

    // Reporte de estadísticas
    printf("\n=== REPORTE DE ANCHO DE BANDA (SATELLITE) ===\n");
    printf("Tiempo total: %.2f segundos\n", elapsed_sec);
    printf("DOWNLINK (video TX):\n");
    printf("  - Bytes transmitidos: %lu\n", (unsigned long)bytes_tx);
    printf("  - Ancho de banda: %.2f kbps\n", downlink_kbps);
    printf("UPLINK (input RX):\n");
    printf("  - Bytes recibidos: %lu\n", (unsigned long)bytes_rx);
    printf("  - Ancho de banda: %.2f kbps\n", uplink_kbps);
    printf("==============================================\n\n");

    g_running = 0;
}

/**
 * PASO 4: HILO DE DOOM (35 Hz)
 *
 * Responsabilidades:
 * - Inicializar el motor de DOOM (D_DoomMain)
 * - El motor corre su propio loop a 35 Hz
 * - Los frames se escriben automáticamente en el ring buffer desde I_FinishUpdate
 * - Recibe input del uplink thread via cola compartida
 *
 * NOTA: D_DoomMain() contiene un loop infinito (D_DoomLoop) que solo
 *       termina cuando se cierra el juego (ESC, quit, etc.)
 */

// Declaraciones externas del motor DOOM
extern void D_DoomMain(void);
extern int myargc;
extern char **myargv;

// Funciones de inicialización de DOOM (necesarias antes de D_DoomMain)
extern void M_FindResponseFile(void);
extern void M_SetExeDir(void);

void* doom_thread_func() {
    printf("[DOOM THREAD] Iniciando en core 0...\n");

    // TODO: Establecer afinidad de CPU (core 0)
    // cpu_set_t cpuset;
    // CPU_ZERO(&cpuset);
    // CPU_SET(0, &cpuset);
    // pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // Preparar argumentos para DOOM
    // IMPORTANTE: Duplicar cadenas en heap porque DOOM puede modificarlas o liberarlas
    // Las cadenas literales están en .rodata (read-only) y causarían segfault
    static char *doom_argv[8];
    doom_argv[0] = strdup("doom-satellite");
    doom_argv[1] = strdup("-iwad");
    doom_argv[2] = strdup("/home/kaox/Documentos/5ºGrao/2ºCuatri/hackudc/HackUDC-2026/freedoom1.wad");
    doom_argv[3] = strdup("-window");
    doom_argv[4] = strdup("-nogui");
    doom_argv[5] = strdup("-nomusic");
    doom_argv[6] = strdup("-nosound");
    doom_argv[7] = NULL;

    myargc = 7;
    myargv = doom_argv;

    printf("[DOOM THREAD] Iniciando motor DOOM...\n");
    printf("[DOOM THREAD] IWAD: %s\n", doom_argv[2]);

    // CRÍTICO: Inicializar subsistemas de DOOM antes de D_DoomMain()
    // M_FindResponseFile() procesa archivos de respuesta (@response.txt)
    // M_SetExeDir() establece exedir (requerido por M_SetConfigDir)
    M_FindResponseFile();
    M_SetExeDir();

    // Iniciar motor DOOM
    // Esta función contiene un loop infinito que corre a 35 Hz
    // Solo retorna cuando el juego termina (ESC, quit, señal, etc.)
    D_DoomMain();

    printf("[DOOM THREAD] Motor DOOM terminado\n");

    // Señalizar terminación global
    g_running = 0;

    return NULL;
}

/**
 * PASO 5: HILO DE ENCODER/TRANSMISIÓN (20 FPS)
 *
 * Responsabilidades:
 * - Leer frames del ring buffer cuando estén disponibles
 * - Encodear RGB→H.264 usando encoder.c
 * - Enviar NAL units por downlink.c
 * - Controlar timing a 20 FPS (50 ms entre frames)
 *
 * IMPLEMENTACIÓN:
 * - Establecer afinidad de CPU al core 1
 * - Inicializar encoder con encoder_init()
 * - Loop que busca slots READY en el ring buffer
 * - Marca slot como READING, encodea, transmite
 * - Marca slot como EMPTY cuando termina
 * - Avanza read_index de forma circular
 */

void* encoder_thread_func(void* arg) {
    printf("[ENCODER THREAD] Iniciando en core 1...\n");

    // TODO: Establecer afinidad de CPU (core 1)
    // cpu_set_t cpuset;
    // CPU_ZERO(&cpuset);
    // CPU_SET(1, &cpuset);
    // pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // TEMPORAL: Saltarse inicialización de encoder para enviar frames raw
    if (encoder_init() != 0) {
        fprintf(stderr, "[ENCODER THREAD] Error: Fallo al inicializar encoder\n");
        g_running = 0;
        return NULL;
    }
    printf("[ENCODER THREAD] Modo RAW activo (sin encoding H.264)\n");

    // Configuración de timing para 20 FPS
    const long frame_ns = 50000000;  // 50 ms = 1000000000 / 20
    struct timespec next_frame;
    clock_gettime(CLOCK_MONOTONIC, &next_frame);

    // Estadísticas
    uint32_t frames_encoded = 0;
    uint32_t frames_dropped = 0;

    printf("[ENCODER THREAD] Loop principal iniciado (20 FPS)\n");

    // Buffer temporal para frame grayscale (8-bit indexed)
    uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT];
    uint32_t frame_num;

    // Loop principal de encoding y transmisión
    while (g_running) {
        // Intentar leer frame del ring buffer compartido
        if (ringbuffer_read_frame(frame_buffer, &frame_num)) {
            printf("[ENCODER THREAD] Recogido frame #%u del ring buffer\n", frame_num);

            // =========================================================================
            // TEMPORAL: Enviar frame raw directamente sin encoding H.264
            // =========================================================================
            // El buffer contiene grayscale de 176x144 bytes del ringbuffer
            // // size_t frame_size = FRAME_WIDTH * FRAME_HEIGHT;
            // // int bytes_sent = downlink_send_raw_frame(frame_buffer, frame_size);

            // // if (bytes_sent > 0) {
            //     frames_encoded++;

                // Log periódico cada 5 segundos (100 frames @ 20 FPS)
                if (frames_encoded % 100 == 0) {
                    printf("[ENCODER THREAD] Frames encoded: %u, dropped: %u\n",
                           frames_encoded, frames_dropped);
                }
            } else {
                fprintf(stderr, "[ENCODER THREAD] Error al enviar frame raw %u\n", frame_num);
            }

            // =========================================================================
            // CÓDIGO ORIGINAL CON ENCODING H.264 (comentado temporalmente):
            // =========================================================================
            // Encodear frame RGB → H.264 NAL units
            encoder_output_t output;
            int num_nals = encoder_encode_frame(frame_buffer, &output);

            if (num_nals > 0) {
                // Transmitir los NAL units por downlink
                // El timestamp RTP se calcula en unidades de 90 kHz (estándar H.264)
                // frame_num * (90000 / 20) = frame_num * 4500 ticks por frame @ 20 FPS
                uint32_t rtp_timestamp = frame_num * 4500;

                downlink_send_nals(output.nals, output.nal_sizes, output.num_nals, rtp_timestamp);
            //     frames_encoded++;

            //     // Log periódico cada 5 segundos (100 frames @ 20 FPS)
            //     if (frames_encoded % 100 == 0) {
            //         printf("[ENCODER THREAD] Frames raw sent: %u, dropped: %u\n",
            //                frames_encoded, frames_dropped);
            //     }
            // } else {
            //     fprintf(stderr, "[ENCODER THREAD] Error al enviar frame raw %u\n", frame_num);
            // }

            // =========================================================================
            // CÓDIGO ORIGINAL CON ENCODING H.264 (comentado temporalmente):
            // =========================================================================
            // Encodear frame RGB → H.264 NAL units
            encoder_output_t output;
            int num_nals = encoder_encode_frame(frame_buffer, &output);

            if (num_nals > 0) {
                // Transmitir los NAL units por downlink
                // El timestamp RTP se calcula en unidades de 90 kHz (estándar H.264)
                // frame_num * (90000 / 20) = frame_num * 4500 ticks por frame @ 20 FPS
                uint32_t rtp_timestamp = frame_num * 4500;

                downlink_send_nals(output.nals, output.nal_sizes, output.num_nals, rtp_timestamp);
                frames_encoded++;

                // Log periódico cada 5 segundos (100 frames @ 20 FPS)
                if (frames_encoded % 100 == 0) {
                    printf("[ENCODER THREAD] Frames encoded: %u, dropped: %u\n",
                           frames_encoded, frames_dropped);
                }
            } else if (num_nals < 0) {
                fprintf(stderr, "[ENCODER THREAD] Error al encodear frame %u\n", frame_num);
            }
            // num_nals == 0 significa delayed frame, no es error
        }

        // Dormir hasta el siguiente frame (timing preciso a 20 FPS)
        next_frame.tv_nsec += frame_ns;
        if (next_frame.tv_nsec >= 1000000000) {
            next_frame.tv_sec++;
            next_frame.tv_nsec -= 1000000000;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_frame, NULL);
    }

    printf("[ENCODER THREAD] Finalizando...\n");
    printf("[ENCODER THREAD] Estadísticas finales: %u frames raw sent, %u dropped\n",
           frames_encoded, frames_dropped);

    // TEMPORAL: Saltarse cleanup del encoder ya que no se inicializó
    encoder_shutdown();

    printf("[ENCODER THREAD] Terminado\n");
    return NULL;
}

/**
 * PASO 6: FUNCIÓN MAIN
 *
 * Secuencia de inicialización:
 * 1. Configurar handler de señales (SIGINT, SIGTERM)
 * 2. Inicializar ring buffer (todos los slots a EMPTY)
 * 3. Inicializar subsistemas (uplink, downlink)
 * 4. Crear hilos de DOOM y encoder
 * 5. Esperar terminación con pthread_join()
 * 6. Cleanup de todos los recursos
 */

int main(int argc, char** argv) {
    printf("=== DOOM SATELLITE - COMPONENTE SATÉLITE ===\n");
    printf("Versión minimalista iniciando...\n\n");

    // PASO 6.0 - Parsear argumentos de línea de comandos
    const char* ground_ip = GROUND_IP;      // Valor por defecto del protocol.h
    int uplink_port = UPLINK_PORT;          // Puerto para recibir comandos
    int downlink_port = DOWNLINK_PORT;      // Puerto para enviar vídeo

    if (argc >= 4) {
        // Formato: ./doom-sat <GROUND_IP> <UPLINK_PORT> <DOWNLINK_PORT>
        ground_ip = argv[1];
        uplink_port = atoi(argv[2]);
        downlink_port = atoi(argv[3]);
        printf("[MAIN] Configuración desde argumentos:\n");
        printf("[MAIN]   - IP estación terrestre: %s\n", ground_ip);
        printf("[MAIN]   - Puerto uplink (recibir): %d\n", uplink_port);
        printf("[MAIN]   - Puerto downlink (enviar): %d\n", downlink_port);
    } else if (argc > 1) {
        // Solo IP proporcionada
        ground_ip = argv[1];
        printf("[MAIN] IP estación terrestre: %s (desde argv)\n", ground_ip);
        printf("[MAIN] Puertos por defecto: uplink=%d, downlink=%d\n", uplink_port, downlink_port);
    } else {
        printf("[MAIN] Usando configuración por defecto:\n");
        printf("[MAIN]   - IP estación terrestre: %s\n", ground_ip);
        printf("[MAIN]   - Puerto uplink (recibir): %d\n", uplink_port);
        printf("[MAIN]   - Puerto downlink (enviar): %d\n", downlink_port);
        printf("[MAIN] Uso: %s <GROUND_IP> <UPLINK_PORT> <DOWNLINK_PORT>\n", argv[0]);
        printf("[MAIN] Ejemplo: %s 192.168.1.100 5000 5001\n", argv[0]);
    }

    // PASO 6.1 - Configurar señales para apagado limpio y capturar tiempo de inicio
    clock_gettime(CLOCK_MONOTONIC, &g_start_time);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // PASO 6.2 - El ring buffer se inicializa automáticamente en i_video.c
    // mediante ringbuffer_init() cuando DOOM arranca

    // PASO 6.3 - Inicializar subsistemas
    // uplink: recibe comandos del jugador por UDP
    printf("[MAIN] Inicializando uplink en puerto %d...\n", uplink_port);
    if (uplink_init(uplink_port) < 0) {
        fprintf(stderr, "[ERROR] Fallo al inicializar uplink\n");
        return 1;
    }

    // downlink: envía vídeo H.264 a estación terrestre
    printf("[MAIN] Inicializando downlink hacia %s:%d...\n", ground_ip, downlink_port);
    if (downlink_init(ground_ip, downlink_port) < 0) {
        fprintf(stderr, "[ERROR] Fallo al inicializar downlink\n");
        uplink_shutdown();
        return 1;
    }

    // PASO 6.4 - Crear hilos
    // Hilo 1: DOOM a 35 Hz en core 0
    printf("[MAIN] Creando hilo de DOOM...\n");
    if (pthread_create(&g_doom_thread, NULL, doom_thread_func, NULL) != 0) {
        fprintf(stderr, "[ERROR] No se pudo crear hilo de DOOM\n");
        uplink_shutdown();
        downlink_shutdown();
        return 1;
    }

    // Hilo 2: Encoder/Transmisión a 20 FPS en core 1
    printf("[MAIN] Creando hilo de encoder...\n");
    if (pthread_create(&g_encoder_thread, NULL, encoder_thread_func, NULL) != 0) {
        fprintf(stderr, "[ERROR] No se pudo crear hilo de encoder\n");
        g_running = 0;
        pthread_join(g_doom_thread, NULL);
        uplink_shutdown();
        downlink_shutdown();
        return 1;
    }

    printf("[MAIN] Sistema en ejecución. Ctrl+C para terminar.\n");

    // PASO 6.5 - Esperar terminación de hilos
    // pthread_join() bloquea hasta que el hilo termine
    pthread_join(g_doom_thread, NULL);
    pthread_join(g_encoder_thread, NULL);

    // PASO 6.6 - Cleanup de recursos
    printf("\n[MAIN] Limpiando recursos...\n");
    uplink_shutdown();
    downlink_shutdown();

    printf("[MAIN] Apagado completo.\n");
    return 0;
}

/**
 * NOTAS DE IMPLEMENTACIÓN MINIMALISTA:
 *
 * 1. PRIORIDADES DE DESARROLLO:
 *    a) Implementar protocol.h con todas las constantes (FRAME_WIDTH, etc.)
 *    b) Implementar encoder.c (wrapper básico de libx264)
 *    c) Implementar downlink.c (empaquetado RTP y envío UDP)
 *    d) Implementar uplink.c (recepción UDP de comandos)
 *    e) Adaptar doomgeneric/doomgeneric_satellite.c para usar ring buffer
 *
 * 2. SIMPLIFICACIONES INICIALES:
 *    - Ring buffer fijo de 3 slots
 *    - Sin estadísticas ni logging avanzado
 *    - Sin manejo de errores de red sofisticado
 *    - Logging simple con printf (stderr para errores)
 *
 * 3. COMPILACIÓN LOCAL:
 *    gcc -o satellite_main main.c encoder.c downlink.c uplink.c \
 *        doom/doomgeneric.c doom/doomgeneric_satellite.c \
 *        doom/doom/*.c doom/dummy.c \
 *        -lpthread -lx264 -I./doom -I./ -DFEATURE_SOUND=0
 *
 * 4. INTEGRACIÓN CON RING BUFFER (doomgeneric_satellite.c):
 *
 *    En DG_DrawFrame():
 *    ```c
 *    extern RingBufferSlot g_ring_buffer[];
 *    extern atomic_int g_write_index;
 *
 *    void DG_DrawFrame() {
 *        int slot = atomic_load(&g_write_index);
 *        int expected = 0; // EMPTY
 *
 *        // Intentar obtener slot vacío con CAS
 *        if (atomic_compare_exchange_strong(&g_ring_buffer[slot].status, &expected, 1)) {
 *            // WRITING
 *            memcpy(g_ring_buffer[slot].frame_data, DG_ScreenBuffer,
 *                   FRAME_WIDTH * FRAME_HEIGHT * 3);
 *            g_ring_buffer[slot].frame_number = current_frame++;
 *            atomic_store(&g_ring_buffer[slot].status, 2); // READY
 *            atomic_store(&g_write_index, (slot + 1) % RING_BUFFER_SIZE);
 *        }
 *        // Si falla, frame se dropea (encoder no procesa a 35 FPS)
 *    }
 *    ```
 *
 * 5. TESTING CON SIMULACIÓN DE LATENCIA:
 *    # Simular 30ms RTT en loopback
 *    sudo tc qdisc add dev lo root netem delay 15ms
 *
 *    # Simular pérdida de paquetes 1%
 *    sudo tc qdisc change dev lo root netem delay 15ms loss 1%
 *
 *    # Limpiar reglas
 *    sudo tc qdisc del dev lo root
 */