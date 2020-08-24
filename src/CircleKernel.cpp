#include "CircleKernel.h"
#include <circle/startup.h>
#include <circle/cputhrottle.h>

u8 CircleKernel::_buttonState = 0;

CircleKernel::CircleKernel()
    : CStdlibAppStdio("BearGB")
{
}

bool CircleKernel::Initialize()
{
    CCPUThrottle::Get()->SetSpeed(CPUSpeedMaximum);

    // Note: Skipping pins 0x12 and 0x13 for audio usage
    unsigned dpi24pins[] =
    {
        0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b
    };

    // put all the pins in AltMode2 for the GPI case
    for (const unsigned pin : dpi24pins)
    {
        CGPIOPin(pin, GPIOModeAlternateFunction2);
    }

    bool result = CStdlibAppStdio::Initialize();

    bool foundGamePad = false;
    for (unsigned deviceIdx = 1; 1; deviceIdx++)
    {
        CString deviceName;
        deviceName.Format ("upad%u", deviceIdx);

        CUSBGamePadDevice *gamePad =
            (CUSBGamePadDevice *)mDeviceNameService.GetDevice(deviceName, false /*bBlockDevice*/);
        if (gamePad == nullptr)
        {
            break;
        }

        const TGamePadState *pState = gamePad->GetInitialState();
        assert (pState != 0);

        gamePad->RegisterStatusHandler(GamePadStatusHandler);

        foundGamePad = true;
    }

    if (!foundGamePad)
    {
        mLogger.Write(GetKernelName(), LogPanic, "Gamepad not found");
        result = false;
    }

    return result;
}

bool CircleKernel::IsButtonPressed(HostButton button)
{
    return (_buttonState & button) != 0;
}

HostExitCode CircleKernel::RunApp(int argc, const char *argv[])
{
    bool running = true;

    _gameBoy.reset(new GameBoy(GameBoyModel::GameBoy, "dkl.gb", this));

    CBcmFrameBuffer *frameBuffer = mScreen.GetFrameBuffer();
    TScreenColor *frameBufferBuffer = (TScreenColor *)(uintptr)frameBuffer->GetBuffer();
    u32 pitch = frameBuffer->GetPitch() / sizeof(TScreenColor);
    u32 width = frameBuffer->GetWidth();
    u32 height = frameBuffer->GetHeight();

    while (running)
    {
        _gameBoy->RunOneFrame();
        u32 *gameBoyPixels = _gameBoy->GetPixelBuffer();

        int cx = width > 160 ? width / 2 - 160 / 2 : 0;
        int cy = height > 144 ? height / 2 - 144 / 2 : 0;

        for (int x = 0; x < 160; x++)
        for (int y = 0; y < 144; y++)
        {
            u32 gbColor = gameBoyPixels[y * 160 + x];
            u8 r = gbColor >> 24;
            u8 g = gbColor >> 16;
            u8 b = gbColor >> 8;

            frameBufferBuffer[(x + cx) + ((y + cy) * pitch)] = COLOR16(r >> 3, g >> 3, b >> 3);
        }
    }

    return HostExitCode::Success;
}

void CircleKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState)
{
    _buttonState = HostButton::None;
    _buttonState |= (pState->buttons & GamePadButtonRight) != 0 ? HostButton::Right : HostButton::None;
    _buttonState |= (pState->buttons & GamePadButtonLeft) != 0 ? HostButton::Left : HostButton::None;
    _buttonState |= (pState->buttons & GamePadButtonUp) != 0 ? HostButton::Up : HostButton::None;
    _buttonState |= (pState->buttons & GamePadButtonDown) != 0 ? HostButton::Down : HostButton::None;
    _buttonState |= (pState->buttons & GamePadButtonA) != 0 ? HostButton::A : HostButton::None;
    _buttonState |= ((pState->buttons & GamePadButtonB) != 0) || ((pState->buttons & GamePadButtonX) != 0) ? HostButton::B : HostButton::None;
    _buttonState |= (pState->buttons & GamePadButtonSelect) != 0 ? HostButton::Select : HostButton::None;
    _buttonState |= (pState->buttons & GamePadButtonStart) != 0 ? HostButton::Start : HostButton::None;
}

int main(int argc, const char *argv[])
{
    CircleKernel kernel;

    if (!kernel.Initialize())
    {
        halt();
        return EXIT_HALT;
    }

    auto exitCode = kernel.RunApp(argc, argv);
    switch (exitCode)
    {
        case HostExitCode::Failure:
            reboot();
            return EXIT_REBOOT;

        case HostExitCode::Success:
        default:
            halt();
            return EXIT_HALT;
    }
}