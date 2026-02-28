#include "encoder.h"
#include "protocol.h"
#include <x264.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Estructura interna del encoder
 */
typedef struct {
    x264_t *encoder;              // Contexto del encoder x264
    x264_param_t param;           // Parámetros de configuración
    x264_picture_t pic_in;        // Frame de entrada (YUV420p)
    x264_picture_t pic_out;       // Frame de salida

    // Buffers para conversión RGB -> YUV420p
    uint8_t *yuv_buffer;          // Buffer YUV420p completo
    int yuv_size;                 // Tamaño del buffer YUV

    // Output NAL units (se reutilizan entre llamadas)
    x264_nal_t *nals;
    int num_nals;
} encoder_ctx_t;

static encoder_ctx_t ctx = {0};

/**
 * Conversión manual RGB -> YUV420p
 *
 * Formula ITU-R BT.601:
 * Y  =  0.299*R + 0.587*G + 0.114*B
 * Cb = -0.169*R - 0.331*G + 0.500*B + 128
 * Cr =  0.500*R - 0.419*G - 0.081*B + 128
 */
static void rgb_to_yuv420p(const uint8_t *rgb, x264_picture_t *pic, int width, int height) {
    uint8_t *y_plane = pic->img.plane[0];
    uint8_t *u_plane = pic->img.plane[1];
    uint8_t *v_plane = pic->img.plane[2];

    int y_stride = pic->img.i_stride[0];
    int uv_stride = pic->img.i_stride[1];

    // Convertir RGB a Y (luminancia) para cada píxel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int rgb_idx = (y * width + x) * 3;
            uint8_t r = rgb[rgb_idx + 0];
            uint8_t g = rgb[rgb_idx + 1];
            uint8_t b = rgb[rgb_idx + 2];

            // Calcular Y
            int Y = (299 * r + 587 * g + 114 * b) / 1000;
            y_plane[y * y_stride + x] = Y;
        }
    }

    // Convertir RGB a UV (croma) con submuestreo 2x2
    // En YUV420p, cada 4 píxeles (2×2) comparten los mismos valores U y V
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x += 2) {
            // Promediar 4 píxeles RGB para obtener un solo valor UV
            int r_sum = 0, g_sum = 0, b_sum = 0;

            for (int dy = 0; dy < 2 && (y + dy) < height; dy++) {
                for (int dx = 0; dx < 2 && (x + dx) < width; dx++) {
                    int rgb_idx = ((y + dy) * width + (x + dx)) * 3;
                    r_sum += rgb[rgb_idx + 0];
                    g_sum += rgb[rgb_idx + 1];
                    b_sum += rgb[rgb_idx + 2];
                }
            }

            uint8_t r_avg = r_sum / 4;
            uint8_t g_avg = g_sum / 4;
            uint8_t b_avg = b_sum / 4;

            // Calcular Cb (U) y Cr (V)
            int U = (-169 * r_avg - 331 * g_avg + 500 * b_avg) / 1000 + 128;
            int V = ( 500 * r_avg - 419 * g_avg -  81 * b_avg) / 1000 + 128;

            // Clamp a rango válido [0, 255]
            U = U < 0 ? 0 : (U > 255 ? 255 : U);
            V = V < 0 ? 0 : (V > 255 ? 255 : V);

            int uv_x = x / 2;
            int uv_y = y / 2;
            u_plane[uv_y * uv_stride + uv_x] = U;
            v_plane[uv_y * uv_stride + uv_x] = V;
        }
    }
}

int encoder_init(void) {
    // Configurar parámetros por defecto para preset ultrafast
    if (x264_param_default_preset(&ctx.param, "ultrafast", "zerolatency") < 0) {
        fprintf(stderr, "Error: Failed to set x264 preset\n");
        return -1;
    }

    // Configuración de resolución
    ctx.param.i_width = FRAME_WIDTH;
    ctx.param.i_height = FRAME_HEIGHT;

    // Configuración de bitrate
    ctx.param.rc.i_bitrate = 192;  // 192 kbps (rango 128-256 kbps según spec)
    ctx.param.rc.i_rc_method = X264_RC_ABR;  // Average Bitrate

    // Profile baseline: sin B-frames, mínima latencia
    x264_param_apply_profile(&ctx.param, "baseline");

    // Configuración de GOP (Group of Pictures)
    ctx.param.i_keyint_max = 60;    // Keyframe cada 60 frames (~3 segundos a 20 FPS)
    ctx.param.i_keyint_min = 20;    // Keyframe mínimo cada 1 segundo
    ctx.param.b_intra_refresh = 0;  // Sin intra refresh (usar keyframes explícitos)

    // Sin B-frames para latencia mínima (ya establecido por baseline)
    ctx.param.i_bframe = 0;

    // Configuración de threading (usar 1 thread para determinismo)
    ctx.param.i_threads = 1;
    ctx.param.b_sliced_threads = 0;

    // Sin lookahead (ya establecido por zerolatency)
    ctx.param.rc.i_lookahead = 0;

    // Configuración de FPS
    ctx.param.i_fps_num = 20;  // 20 FPS según spec
    ctx.param.i_fps_den = 1;
    ctx.param.i_timebase_num = 1;
    ctx.param.i_timebase_den = 1000;  // Timebase en milisegundos

    // Desactivar parámetros innecesarios para reducir overhead
    ctx.param.b_cabac = 0;  // Baseline no usa CABAC
    ctx.param.b_deblocking_filter = 1;  // Mantener filtro de deblocking para calidad

    // Configuración de anexos (AnnexB para streaming RTP)
    ctx.param.b_annexb = 1;
    ctx.param.b_repeat_headers = 1;  // Repetir SPS/PPS para resiliencia

    // Abrir el encoder
    ctx.encoder = x264_encoder_open(&ctx.param);
    if (!ctx.encoder) {
        fprintf(stderr, "Error: Failed to open x264 encoder\n");
        return -1;
    }

    // Alocar picture de entrada (YUV420p)
    if (x264_picture_alloc(&ctx.pic_in, X264_CSP_I420, FRAME_WIDTH, FRAME_HEIGHT) < 0) {
        fprintf(stderr, "Error: Failed to allocate x264 picture\n");
        x264_encoder_close(ctx.encoder);
        ctx.encoder = NULL;
        return -1;
    }

    // Calcular tamaño del buffer YUV420p
    // Y plane: width × height
    // U plane: (width/2) × (height/2)
    // V plane: (width/2) × (height/2)
    ctx.yuv_size = FRAME_WIDTH * FRAME_HEIGHT * 3 / 2;
    ctx.yuv_buffer = malloc(ctx.yuv_size);
    if (!ctx.yuv_buffer) {
        fprintf(stderr, "Error: Failed to allocate YUV buffer\n");
        x264_picture_clean(&ctx.pic_in);
        x264_encoder_close(ctx.encoder);
        ctx.encoder = NULL;
        return -1;
    }

    printf("[encoder] Initialized: %dx%d @ %d FPS, bitrate=%d kbps, preset=ultrafast, tune=zerolatency\n",
           FRAME_WIDTH, FRAME_HEIGHT, 20, 192);

    return 0;
}

int encoder_encode_frame(const uint8_t *rgb_data, encoder_output_t *output) {
    if (!ctx.encoder || !rgb_data || !output) {
        fprintf(stderr, "Error: Invalid encoder state or parameters\n");
        return -1;
    }

    // Convertir RGB -> YUV420p
    rgb_to_yuv420p(rgb_data, &ctx.pic_in, FRAME_WIDTH, FRAME_HEIGHT);

    // Configurar el PTS (Presentation Time Stamp)
    // Se incrementa automáticamente basado en el número de frames
    static int64_t frame_count = 0;
    ctx.pic_in.i_pts = frame_count++;

    // Codificar el frame
    x264_picture_t pic_out;
    int frame_size = x264_encoder_encode(ctx.encoder, &ctx.nals, &ctx.num_nals, &ctx.pic_in, &pic_out);

    if (frame_size < 0) {
        fprintf(stderr, "Error: x264_encoder_encode failed\n");
        return -1;
    }

    // Si no se produjo output (delayed frames), retornar 0
    if (frame_size == 0 || ctx.num_nals == 0) {
        output->nals = NULL;
        output->nal_sizes = NULL;
        output->num_nals = 0;
        return 0;
    }

    // Preparar la salida
    // Los NAL units están en ctx.nals, que es manejado internamente por x264
    // Necesitamos extraer los punteros y tamaños
    output->num_nals = ctx.num_nals;

    // Alocar arrays temporales para los punteros y tamaños
    static uint8_t *nal_ptrs[32];  // Suficiente para la mayoría de casos
    static size_t nal_sizes[32];

    if (ctx.num_nals > 32) {
        fprintf(stderr, "Warning: Too many NAL units (%d), truncating to 32\n", ctx.num_nals);
        output->num_nals = 32;
    }

    // Extraer los NAL units
    for (int i = 0; i < output->num_nals; i++) {
        nal_ptrs[i] = ctx.nals[i].p_payload;
        nal_sizes[i] = ctx.nals[i].i_payload;
    }

    output->nals = nal_ptrs;
    output->nal_sizes = nal_sizes;

    return output->num_nals;
}

void encoder_shutdown(void) {
    if (!ctx.encoder) {
        return;  // Ya estaba cerrado
    }

    // Flush de frames pendientes (delayed frames)
    printf("[encoder] Flushing delayed frames...\n");
    int flush_count = 0;
    while (1) {
        x264_picture_t pic_out;
        int frame_size = x264_encoder_encode(ctx.encoder, &ctx.nals, &ctx.num_nals, NULL, &pic_out);

        if (frame_size <= 0) {
            break;  // No más frames pendientes
        }

        flush_count++;
    }

    if (flush_count > 0) {
        printf("[encoder] Flushed %d delayed frames\n", flush_count);
    }

    // Liberar recursos
    x264_picture_clean(&ctx.pic_in);

    if (ctx.yuv_buffer) {
        free(ctx.yuv_buffer);
        ctx.yuv_buffer = NULL;
    }

    x264_encoder_close(ctx.encoder);
    ctx.encoder = NULL;

    printf("[encoder] Shutdown complete\n");
}
