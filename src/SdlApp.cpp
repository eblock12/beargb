#include "SdlApp.h"

#include <cstring>
#include <iostream>

// GPI internal screen res
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

SdlApp::SdlApp()
{
    _window = nullptr;
    _audioDevice = 0;
}

SdlApp::~SdlApp()
{
    if (_audioDevice > 0)
    {
        SDL_CloseAudioDevice(_audioDevice);
    }
    if (_window != nullptr)
    {
        SDL_DestroyWindow(_window);
    }
    SDL_Quit();
}

bool SdlApp::Initialize()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
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

    SDL_AudioSpec requestedAudioSpec;
    memset(&requestedAudioSpec, 0, sizeof(requestedAudioSpec));
    requestedAudioSpec.freq = 44100;
    requestedAudioSpec.format = AUDIO_S16SYS;
    requestedAudioSpec.channels = 2;
    requestedAudioSpec.silence = 0;
    requestedAudioSpec.samples = 4096;
    requestedAudioSpec.callback = nullptr;

    _audioDevice = SDL_OpenAudioDevice(nullptr, 0, &requestedAudioSpec, &_audioSpec, 0);
    if (_audioDevice > 0)
    {
        SDL_PauseAudioDevice(_audioDevice, 0); // start playing
        std::cout << "Started playing: " << SDL_GetAudioDeviceName(2,0) << std::endl;
        std::cout << "frequency " << _audioSpec.freq << std::endl;
    }
    else
    {
        return false;
    }

    return true;
}

bool SdlApp::IsButtonPressed(HostButton button)
{
    if (_keyboardState == nullptr)
    {
        return false;
    }

    switch (button)
    {
        case HostButton::Right:
            return _keyboardState[SDL_SCANCODE_RIGHT];
        case HostButton::Left:
            return _keyboardState[SDL_SCANCODE_LEFT];
        case HostButton::Up:
            return _keyboardState[SDL_SCANCODE_UP];
        case HostButton::Down:
            return _keyboardState[SDL_SCANCODE_DOWN];
        case HostButton::A:
            return _keyboardState[SDL_SCANCODE_X];
        case HostButton::B:
            return _keyboardState[SDL_SCANCODE_Z];
        case HostButton::Select:
            return _keyboardState[SDL_SCANCODE_BACKSPACE];
        case HostButton::Start:
            return _keyboardState[SDL_SCANCODE_RETURN];
        default:
            return false;
    }

    return false;
}

HostExitCode SdlApp::RunApp(int argc, const char *argv[])
{
    const char *romFile = (argc > 1) ? argv[1] : "tetris.gb";
    _gameBoy.reset(new GameBoy(GameBoyModel::GameBoy, romFile, this));

    SDL_Event event;
    bool running = true;

    u32 lastTicks = SDL_GetTicks();

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
            u32 gbColor = gameBoyPixels[y * 160 + x];

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

        _keyboardState = SDL_GetKeyboardState(nullptr);
    }

    return HostExitCode::Success;
}

void SdlApp::QueueAudio(s16 *buffer, u32 sampleCount)
{
    SDL_QueueAudio(_audioDevice, buffer, sampleCount * 2 * sizeof(s16));
}

void SdlApp::SyncAudio()
{
    u32 maxBytes = 2940 * 8;

    while (SDL_GetQueuedAudioSize(_audioDevice) > maxBytes)
    {
    }
}

int main(int argc, char *argv[])
{
    SdlApp app;

    if (!app.Initialize())
    {
        return 1;
    }

    return (int)app.RunApp(argc, (const char **)argv);
}