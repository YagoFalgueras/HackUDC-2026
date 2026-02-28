/*
 * encoder.c — Stub passthrough del codificador H.264
 *
 * Responsabilidad:
 *   Implementación provisional (sin H.264 real). Todas las funciones son
 *   no-ops o retornan éxito inmediato. El encoder real usará libx264.
 */

#include "encoder.h"

int encoder_init(void)
{
    return 0;
}

int encoder_encode_frame(const uint8_t *rgb_data, encoder_output_t *output)
{
    (void)rgb_data;
    output->num_nals = 0;
    return 0;
}

void encoder_shutdown(void)
{
}
