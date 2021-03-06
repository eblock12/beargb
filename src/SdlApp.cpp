#include "SdlApp.h"
#include "OpenRomMenu.h"

#include <cstring>
#include <iostream>

// GPI internal screen res
const int SCREEN_WIDTH = 320*3;
const int SCREEN_HEIGHT = 240*3;

SdlApp::SdlApp()
{
    _window = nullptr;
    _renderer = nullptr;
    _frameTexture = nullptr;
    _audioDevice = 0;
    _menuEnable = false;
}

SdlApp::~SdlApp()
{
    if (_audioDevice > 0)
    {
        SDL_CloseAudioDevice(_audioDevice);
    }
    if (_frameTexture != nullptr)
    {
        SDL_DestroyTexture(_frameTexture);
    }
    if (_renderer != nullptr)
    {
        SDL_DestroyRenderer(_renderer);
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
        SDL_WINDOW_RESIZABLE);
    if (_window == nullptr)
    {
        return false;
    }

    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (_renderer == nullptr)
    {
        return false;
    }
    SDL_RenderSetLogicalSize(_renderer, 160, 144);
    SDL_RenderClear(_renderer);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    _frameTexture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);

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
        //std::cout << "Started playing: " << SDL_GetAudioDeviceName(2,0) << std::endl;
        //std::cout << "frequency " << _audioSpec.freq << std::endl;
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
        case HostButton::Menu:
            return _keyboardState[SDL_SCANCODE_ESCAPE];
        default:
            return false;
    }

    return false;
}

void SdlApp::LoadRomFile(const char *romFile)
{
    _gameBoy.reset(new GameBoy(GameBoyModel::Auto, romFile, this));
    _menuEnable = false;
}

HostExitCode SdlApp::RunApp(int argc, const char *argv[])
{
    const char *romFile = (argc > 1) ? argv[1] : "tetris.gb";
    _gameBoy.reset(new GameBoy(GameBoyModel::Auto, romFile, this));

    SDL_Event event;
    bool running = true;
    bool menuPressed = false;

    OpenRomMenu menu(this);

    while (running)
    {
        if (_menuEnable)
        {
            menu.RunFrame();
        }
        else
        {
            _gameBoy->RunOneFrame();
        }

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

        if (!menuPressed & IsButtonPressed(HostButton::Menu))
        {
            _menuEnable = !_menuEnable;
        }
        menuPressed = IsButtonPressed(HostButton::Menu);
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

void SdlApp::PushVideoFrame(u32 *pixelBuffer)
{
    SDL_UpdateTexture(_frameTexture, nullptr, pixelBuffer, 160 * sizeof(u32));
    SDL_RenderCopy(_renderer, _frameTexture, nullptr, nullptr);
    SDL_RenderPresent(_renderer);
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