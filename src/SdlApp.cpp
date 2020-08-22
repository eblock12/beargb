#include "SdlApp.h"

// GPI internal screen res
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

SdlApp::SdlApp()
{
    _window = nullptr;
}

SdlApp::~SdlApp()
{
    if (_window != nullptr)
    {
        SDL_DestroyWindow(_window);
    }
    SDL_Quit();
}

bool SdlApp::Initialize()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        return false;
    }

    _window = SDL_CreateWindow(
        "BearGB",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0);
    if (_window == nullptr)
    {
        return false;
    }

    _surface = SDL_GetWindowSurface(_window);
    if (_surface == nullptr)
    {
        return false;
    }

    SDL_LockSurface(_surface);
    memset(_surface->pixels, 0, _surface->h * _surface->pitch);
    SDL_UnlockSurface(_surface);
    SDL_UpdateWindowSurface(_window);

    return true;
}

HostExitCode SdlApp::RunApp(int argc, const char *argv[])
{
    const char *romFile = (argc > 1) ? argv[1] : "tetris.gb";
    _gameBoy.reset(new GameBoy(GameBoyModel::GameBoy, romFile));

    SDL_Event event;
    bool running = true;

    while (running)
    {
        _gameBoy->RunOneFrame();
        u32 *gameBoyPixels = _gameBoy->GetPixelBuffer();

        SDL_LockSurface(_surface);

        int cx = 80;
        int cy = 48;
        for (int x = 0; x < 160; x++)
        for (int y = 0; y < 144; y++)
        {
            uint gbColor = gameBoyPixels[y * 160 + x];

            ((u8*)_surface->pixels)[((y + cy) * _surface->w + x + cx) * 4 + 0] = gbColor >> 8;
            ((u8*)_surface->pixels)[((y + cy) * _surface->w + x + cx) * 4 + 1] = gbColor >> 16;
            ((u8*)_surface->pixels)[((y + cy) * _surface->w + x + cx) * 4 + 2] = gbColor >> 24;
            ((u8*)_surface->pixels)[((y + cy) * _surface->w + x + cx) * 4 + 3] = 0; // alpha
        }

        SDL_UnlockSurface(_surface);

        SDL_UpdateWindowSurface(_window);

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }
    }

    return HostExitCode::Success;
}

int main(int argc, const char *argv[])
{
    SdlApp app;

    if (!app.Initialize())
    {
        return 1;
    }

    return (int)app.RunApp(argc, argv);
}