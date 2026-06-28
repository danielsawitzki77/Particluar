#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "App.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // TODO: Initialize and run the application
    SDL_Log("Particluar starting...");

    SDL_Quit();
    return 0;
}
