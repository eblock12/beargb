#pragma once

#include "GameBoy.h"
#include "IHostSystem.h"
#include <SDL.h>
#include <memory>
#include "shared.h"

class SdlApp : public IHostSystem
{
private:
    std::unique_ptr<GameBoy> _gameBoy;

    SDL_Window *_window;
    SDL_Surface *_surface;
    const u8 *_keyboardState;

    SDL_AudioSpec _audioSpec;
    SDL_AudioDeviceID _audioDevice;
public:
    SdlApp();
    ~SdlApp();

    bool Initialize() override;
    bool IsButtonPressed(HostButton button) override;
    HostExitCode RunApp(int argc, const char *argv[]) override;
    void QueueAudio(s16 *buffer, u32 sampleCount) override;
    void SyncAudio() override;
};