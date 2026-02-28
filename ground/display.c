#include "display.h"

#include <stdio.h>

#include <SDL2/SDL.h>

#define SAT_SCREEN_WIDTH   320
#define SAT_SCREEN_HEIGHT  200
#define WINDOW_SCALE       3

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;

int display_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "display_init: SDL_Init falló: %s\n", SDL_GetError());
        return -1;
    }

    window = SDL_CreateWindow(
        "UAC Orbital Relay - Ground Control",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SAT_SCREEN_WIDTH  * WINDOW_SCALE,
        SAT_SCREEN_HEIGHT * WINDOW_SCALE,
        SDL_WINDOW_SHOWN
    );
    if (!window)
    {
        fprintf(stderr, "display_init: SDL_CreateWindow falló: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer)
    {
        fprintf(stderr, "display_init: SDL_CreateRenderer falló: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        SAT_SCREEN_WIDTH, SAT_SCREEN_HEIGHT
    );
    if (!texture)
    {
        fprintf(stderr, "display_init: SDL_CreateTexture falló: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return 0;
}

void display_present_frame(const uint8_t *rgb_buffer)
{
    SDL_UpdateTexture(texture, NULL, rgb_buffer, SAT_SCREEN_WIDTH * 3);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void display_shutdown(void)
{
    if (texture)  { SDL_DestroyTexture(texture);   texture  = NULL; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
    if (window)   { SDL_DestroyWindow(window);     window   = NULL; }
    SDL_Quit();
}
