#include "CircleKernel.h"
#include <circle/startup.h>

CircleKernel::CircleKernel()
    : CStdlibAppStdio("BearGB")
{
}

bool CircleKernel::Initialize()
{
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

    return CStdlibApp::Initialize();
}

HostExitCode CircleKernel::RunApp(int argc, const char *argv[])
{
    bool running = true;

    _gameBoy.reset(new GameBoy(GameBoyModel::GameBoy, "tetris.gb"));

    while (running)
    {
        _gameBoy->RunOneFrame();
        u32 *gameBoyPixels = _gameBoy->GetPixelBuffer();

        int cx = 80;
        int cy = 48;
        for (int x = 0; x < 160; x++)
        for (int y = 0; y < 144; y++)
        {
            u32 gbColor = gameBoyPixels[y * 160 + x];
            u8 r = gbColor >> 24;
            u8 g = gbColor >> 16;
            u8 b = gbColor >> 8;

            mScreen.SetPixel(x + cx, y + cy, COLOR16(r >> 3, g >> 3, b >> 3));
        }
    }

    return HostExitCode::Success;
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