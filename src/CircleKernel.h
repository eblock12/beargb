#pragma once

#include "GameBoy.h"
#include "IHostSystem.h"
#include <circle_stdlib_app.h>
#include <circle/cputhrottle.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbgamepad.h>
#include <circle/pwmsoundbasedevice.h>
#include <queue>
#include <memory>

class CircleKernel : public CStdlibAppStdio, public IHostSystem
{
private:
    static u16 _buttonState;
    std::unique_ptr<GameBoy> _gameBoy;

    CCPUThrottle _cpuThrottle;
    CPWMSoundBaseDevice	_pwmSoundDevice;

    void StartSoundQueue();

    // unused
    TShutdownMode Run() override { return TShutdownMode::ShutdownHalt; }

    static void GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState);

    CGPIOPin _powerButtonPin;
    CGPIOPin _powerEnablePin;

    bool _menuEnable;
public:
    CircleKernel();

    bool Initialize() override;
    bool IsButtonPressed(HostButton button) override;
    void LoadRomFile(const char *romFile) override;
    HostExitCode RunApp(int argc, const char *argv[]) override;
    virtual void QueueAudio(s16 *buffer, u32 sampleCount) override;
    virtual void SyncAudio() override;
    virtual void PushVideoFrame(u32 *pixelBuffer) override;
};