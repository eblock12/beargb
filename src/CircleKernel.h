#pragma once

#include "GameBoy.h"
#include "IHostSystem.h"
#include <circle_stdlib_app.h>
#include <circle/cputhrottle.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbgamepad.h>
#include <memory>

class CircleKernel : public CStdlibAppStdio, public IHostSystem
{
private:
    static u8 _buttonState;
    std::unique_ptr<GameBoy> _gameBoy;

    CCPUThrottle _cpuThrottle;

    // unused
    TShutdownMode Run() override { return TShutdownMode::ShutdownHalt; }

    static void GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState);
public:
    CircleKernel();

    bool Initialize() override;
    bool IsButtonPressed(HostButton button) override;
    HostExitCode RunApp(int argc, const char *argv[]) override;
};