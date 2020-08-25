#pragma once

enum class HostExitCode
{
    Success = 0,
    Failure = 1,
};

enum HostButton : u8
{
    None   = 0x00,
    Right  = 0x01,
    Left   = 0x02,
    Up     = 0x04,
    Down   = 0x08,
    A      = 0x10,
    B      = 0x20,
    Select = 0x40,
    Start  = 0x80
};

class IHostSystem
{
public:
    virtual bool Initialize() = 0;
    virtual bool IsButtonPressed(HostButton button) = 0;
    virtual HostExitCode RunApp(int argc, const char *argv[]) = 0;
    virtual void QueueAudio(s16 *buffer, u32 sampleCount) = 0;
    virtual void SyncAudio() = 0;
};