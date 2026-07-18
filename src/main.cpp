// Particluar — Umbrella Application Entry Point

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "App.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    App app;

    if (!app.Init())
        return 1;

    app.Run();
    app.Shutdown();

    return 0;
}
