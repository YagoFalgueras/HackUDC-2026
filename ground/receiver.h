#ifndef RECEIVER_H
#define RECEIVER_H

#include <stdint.h>

/**
 * Inicializa el subsistema de recepción y decodificación de vídeo.
 *
 * Parámetros:
 *   listen_port: Puerto UDP local para recibir paquetes RTP de vídeo
 *
 * Responsabilidades:
 * - Crea el socket UDP de escucha en modo non-blocking
 * - Vincula el socket al puerto especificado (bind)
 * - Inicializa el buffer de reensamblaje para fragmentos FU-A (RTP fragmentation units)
 * - Inicializa el decoder H.264 de FFmpeg:
 *   • Busca el codec con avcodec_find_decoder(AV_CODEC_ID_H264)
 *   • Reserva AVCodecContext y configura parámetros
 *   • Reserva AVFrame para frames YUV decodificados
 *   • Reserva AVPacket para paquetes H.264
 *   • Reserva buffer RGB de salida (176×144×3 bytes)
 *   • Inicializa SwsContext para conversión YUV420p → RGB24
 *
 * Retorna:
 *   0 en éxito, -1 en error (escribe error a stderr)
 */
int receiver_init(int listen_port);

/**
 * Recibe y procesa paquetes RTP de vídeo, decodifica frames H.264.
 *
 * Responsabilidades:
 * - Hace recvfrom() non-blocking en loop hasta que no haya más paquetes
 * - Para cada paquete recibido:
 *   • Extrae el header RTP con unpack_rtp_header()
 *   • Determina si es un NAL unit completo o un fragmento FU-A
 *   • Si es NAL completo: lo pasa directamente al decoder interno
 *   • Si es fragmento FU-A: lo acumula en el buffer de reensamblaje
 *     hasta completar el NAL (detectado por marker bit en último fragmento)
 *   • Para cada NAL completo:
 *     - Envuelve en AVPacket
 *     - Llama a avcodec_send_packet()
 *     - Llama a avcodec_receive_frame()
 *     - Si produce un frame YUV420p, lo convierte a RGB24 con sws_scale()
 *     - Almacena el frame RGB en el buffer interno
 *
 * Retorna:
 *   Puntero al buffer RGB (176×144×3 bytes) del último frame decodificado,
 *   o NULL si no se completó ningún frame nuevo en esta llamada.
 *
 * Nota: El buffer retornado es interno y se sobrescribe en la siguiente
 * llamada exitosa. El caller debe procesarlo inmediatamente o copiarlo.
 *
 * Esta función debe ser llamada en cada iteración del loop principal.
 */
const uint8_t* receiver_poll(void);

/**
 * Limpia y destruye todos los recursos de recepción y decodificación.
 *
 * Responsabilidades:
 * - Cierra el socket UDP
 * - Libera el buffer de reensamblaje de fragmentos FU-A
 * - Libera AVCodecContext, AVFrame, AVPacket de FFmpeg
 * - Libera SwsContext
 * - Libera buffer RGB de salida
 *
 * Debe ser llamada antes de input_shutdown(), después del loop principal.
 */
void receiver_shutdown(void);

#endif // RECEIVER_H
