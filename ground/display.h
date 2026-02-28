// SPDX-FileCopyrightText: 2026 BFG-I contributors (HackUDC 2026)
// SPDX-License-Identifier: GPL-2.0-only
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/**
 * Inicializa el subsistema de display SDL2.
 *
 * Responsabilidades:
 * - Llama a SDL_Init(SDL_INIT_VIDEO)
 * - Crea SDL_Window escalada (ej: 704×576, que es 4x de QCIF 176×144)
 * - Crea SDL_Renderer con flag SDL_RENDERER_PRESENTVSYNC para sincronización
 * - Crea SDL_Texture en formato SDL_PIXELFORMAT_RGB24 con tamaño nativo QCIF (176×144)
 * - Configura SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0") para nearest-neighbor
 *   y preservar la estética pixelada de DOOM
 *
 * Retorna:
 *   0 en éxito, -1 en error (escribe error a stderr)
 */
int display_init(void);

/**
 * Presenta un frame RGB decodificado en la ventana SDL2.
 *
 * Parámetros:
 *   rgb_buffer: Puntero al buffer RGB (176×144×3 bytes, formato RGB24)
 *
 * Responsabilidades:
 * - Llama a SDL_UpdateTexture() para subir los píxeles a la textura
 * - Llama a SDL_RenderClear() para limpiar el renderer
 * - Llama a SDL_RenderCopy() para copiar la textura al renderer
 *   (SDL2 escala automáticamente al tamaño de la ventana)
 * - Llama a SDL_RenderPresent() para presentar el frame en pantalla
 *
 * Nota: Esta función debe ser rápida ya que se llama en el loop principal.
 * El VSync está habilitado en el renderer para evitar tearing.
 */
void display_present_frame(const uint8_t* rgb_buffer);

/**
 * Limpia y destruye todos los recursos SDL2.
 *
 * Responsabilidades:
 * - Destruye la SDL_Texture
 * - Destruye el SDL_Renderer
 * - Destruye la SDL_Window
 * - Llama a SDL_Quit() para finalizar SDL2
 *
 * Debe ser llamada en orden inverso a la inicialización, después de
 * input_shutdown() y receiver_shutdown().
 */
void display_shutdown(void);

#endif // DISPLAY_H
