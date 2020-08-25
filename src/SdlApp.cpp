#include "SdlApp.h"

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
        std::cout << "Started playing" << std::endl;
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

#include <tuple>


constexpr double A3 = 110.00;
constexpr double B3 = 123.4708;
constexpr double C3 = 130.8128;
constexpr double D3 = 146.8324;
constexpr double E3 = 164.8138;
constexpr double F3 = 174.6141;
constexpr double G3 = 195.9977;
constexpr double A4 = A3 * 2;
constexpr double B4 = B3 * 2;
constexpr double C4 = C3 * 2;
constexpr double D4 = D3 * 2;
constexpr double E4 = E3 * 2;
constexpr double F4 = F4 * 2;
constexpr double G4 = G4 * 2;
constexpr double A5 = A4 * 2;
constexpr double B5 = B4 * 2;

constexpr double QuarterNote = 0.12;
constexpr double HalfNote = QuarterNote * 2.0;
constexpr double WholeNote = HalfNote * 2.0;

constexpr double NoteCutOffPeriod = QuarterNote;

std::pair<double, double> song[] =
{
    { A4, QuarterNote },
    { B4, QuarterNote },
    { C4, QuarterNote },
    { C4, QuarterNote },
    { D4, QuarterNote },
    { C4, QuarterNote },
    { B4, QuarterNote },
    { 0, QuarterNote },

    { E3, QuarterNote },
    { G3, QuarterNote },
    { A4, QuarterNote },
    { A4, QuarterNote },
    { G3, QuarterNote },
    { F3, QuarterNote },
    { G3, QuarterNote },

    { 0, WholeNote },
    { 0, WholeNote },

    { C4, HalfNote },
    { A4, QuarterNote },
    { F3, HalfNote },
    { C3, QuarterNote },
    { C3, QuarterNote },
    { C3, QuarterNote },
    
    { D3, QuarterNote },
    { F3, QuarterNote },
    { A4, QuarterNote },
    { D4, QuarterNote },
    { C4, WholeNote },

    { 0, INFINITY },
};

int songPosition = 0;
double noteFrequency = 0.0;
double noteSecondsLeft = 0.0;
double amp = 0.0;

HostExitCode SdlApp::RunApp(int argc, const char *argv[])
{
    const char *romFile = (argc > 1) ? argv[1] : "tetris.gb";
    _gameBoy.reset(new GameBoy(GameBoyModel::GameBoy, romFile, this));

    SDL_Event event;
    bool running = true;

    s16 outputBuffer[1024] = {};
    u32 bufferPosition = 0;

    double sinePhase = 0.0;

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

        _keyboardState = SDL_GetKeyboardState(nullptr);

/*
        u32 currentTicks = SDL_GetTicks();
        u32 ticksElapsed = currentTicks - lastTicks;
        lastTicks = currentTicks;
        double secondsElapsed = (double)ticksElapsed / 1000.0;
        double vibrato = 0.0;

        if (noteSecondsLeft < -NoteCutOffPeriod)
        {
            std::cout << "Note #" << int(songPosition);
            auto note = song[songPosition++];
            noteFrequency = note.first;
            noteSecondsLeft = note.second;
            //sinePhase = 0;
            std::cout << ": Freq=" << noteFrequency << " Length=" << noteSecondsLeft << std::endl;
        }

        //noteSecondsLeft -= secondsElapsed;

        
        if (SDL_GetQueuedAudioSize(_audioDevice) < (4096 * sizeof(s16)))
        {
            for (int i = 0; i < 1024; i++)
            {
                // note decay
                if (noteSecondsLeft < 0)
                {
                    amp -= (1.0 / 44100.0) * 10;
                    //std::cout << amp << std::endl;

                    if (amp < 0.0)
                    {
                        amp = 0.0;
                    }
                }
                else // note attack
                {
                    amp += (1.0 / 44100.0) * 10;
                    if (amp > 1.0)
                    {
                        amp = 1.0;
                    }
                }

                s16 sample = (s16)(sin(sinePhase * 2 * M_PI) * 16000.0 * amp);
                sample += (s16)(sin(sinePhase * 2.0) / 2.0 * 16000.0 * amp);
                //sample += (s16)(sin(sinePhase * 4.0 + 0.2) * 10000.0 * amp);

                SDL_QueueAudio(_audioDevice, &sample, sizeof(sample));
                sinePhase += ((noteFrequency + 0) * 4) / 44100.0;
                vibrato = sin(sinePhase / 50.0 * 2 * M_PI) * 2.0;

                noteSecondsLeft -= 1.0 / 44100.0;
            }
        }*/
    }

    return HostExitCode::Success;
}

void SdlApp::QueueAudio(s16 *buffer, u32 sampleCount)
{
    SDL_QueueAudio(_audioDevice, buffer, sampleCount * 2 * sizeof(s16));
}

void SdlApp::SyncAudio()
{
    u32 maxBytes = 2759 * 4;

    while (SDL_GetQueuedAudioSize(_audioDevice) > maxBytes)
    {
        SDL_Delay(1);
    }
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