#pragma once

#include "shared.h"

enum class HostExitCode
{
    Success = 0,
    Failure = 1,
};

enum HostButton : u16
{
    None   = 0x00,
    Right  = 0x01,
    Left   = 0x02,
    Up     = 0x04,
    Down   = 0x08,
    A      = 0x10,
    B      = 0x20,
    Select = 0x40,
    Start  = 0x80,

    Menu   = 0x100,
};

// really ((green) & 0x3F) << 5, but to have a 0-31 range for all colors
#define COLOR16(red, green, blue) (((red)&0x1F) << 11 | ((green)&0x1F) << 6 | ((blue)&0x1F))

class IHostSystem
{
public:
    virtual bool Initialize() = 0;
    virtual bool IsButtonPressed(HostButton button) = 0;
    virtual void LoadRomFile(const char *romFile) = 0;
    virtual HostExitCode RunApp(int argc, const char *argv[]) = 0;
    virtual void QueueAudio(s16 *buffer, u32 sampleCount) = 0;
    virtual void SyncAudio() = 0;

    virtual u16 *GetPixelBuffer() = 0;
    virtual void PresentPixelBuffer() = 0;

    virtual void PushVideoFrame(u32 *pixelBuffer) = 0;
};