#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stddef.h>

/**
 * Estructura que contiene los NAL units producidos por el encoder.
 * Cada NAL unit tiene un puntero a sus datos y su tamaño.
 */
typedef struct {
    uint8_t **nals;        // Array de punteros a NAL units
    size_t *nal_sizes;     // Array de tamaños de cada NAL unit
    int num_nals;          // Número de NAL units en este frame
} encoder_output_t;

/**
 * encoder_init - Inicializa el codificador H.264 con libx264
 *
 * Configura libx264 con los siguientes parámetros:
 * - preset: ultrafast (mínimo uso de CPU)
 * - tune: zerolatency (sin lookahead ni buffers)
 * - profile: baseline (sin B-frames, mínima latencia)
 * - Resolución: QCIF (176×144)
 * - Bitrate objetivo: según MAX_BITRATE definido en protocol.h
 * - Keyframe interval: cada N frames (configurable)
 * - Sin B-frames para minimizar latencia
 *
 * Reserva el buffer intermedio para conversión RGB → YUV420p.
 *
 * Returns: 0 en éxito, -1 en error
 */
int encoder_init(void);

/**
 * encoder_encode_frame - Codifica un frame RGB a H.264
 * @rgb_data: Puntero al buffer de píxeles RGB (FRAME_WIDTH × FRAME_HEIGHT × 3 bytes)
 * @output: Puntero a estructura donde se retornarán los NAL units producidos
 *
 * Proceso:
 * 1. Convierte el frame RGB a YUV420p (conversión manual o con swscale)
 * 2. Alimenta el frame a x264_encoder_encode()
 * 3. Retorna los NAL units producidos en la estructura output
 *
 * Nota: Los NAL units retornados son válidos hasta la siguiente llamada a
 * encoder_encode_frame() o encoder_shutdown(). El caller debe procesarlos
 * inmediatamente o copiarlos si necesita retenerlos.
 *
 * Returns: Número de NAL units producidos, o -1 en error
 */
int encoder_encode_frame(const uint8_t *rgb_data, encoder_output_t *output);

/**
 * encoder_shutdown - Finaliza el codificador y libera recursos
 *
 * Realiza flush de frames pendientes en el encoder x264, libera el
 * contexto del encoder y todos los buffers intermedios.
 */
void encoder_shutdown(void);

#endif /* ENCODER_H */
