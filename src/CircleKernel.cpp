#include "CircleKernel.h"
#include "OpenRomMenu.h"
#include <memory.h>
#include <circle/startup.h>
#include <circle/cputhrottle.h>

constexpr unsigned SoundSampleRate = 44100;
constexpr unsigned SoundChunkSize = 2000;
constexpr unsigned int SoundQueueSize = 100; // milliseconds

constexpr unsigned ScreenWidth = 160;
constexpr unsigned ScreenHeight = 144;

u16 CircleKernel::_buttonState = 0;

CircleKernel::CircleKernel() :
    _kernelName("BearGB"),
    _partitionName("SD:"),
    _timer(&_interruptSystem),
    _logger(_kernelOptions.GetLogLevel(), &_timer),
    _usbHciDevice(&_interruptSystem, &_timer),
    _emmcDevice(&_interruptSystem, &_timer, &_actLED),
    _frameBuffer(nullptr),
    _pwmSoundDevice(&_interruptSystem, SoundSampleRate, SoundChunkSize),
    _powerButtonPin(0x1A, GPIOModeInput),
    _powerEnablePin(0x1B, GPIOModeOutput)
{
}

bool CircleKernel::Initialize()
{
    // request max clock speed for now
    CCPUThrottle::Get()->SetSpeed(CPUSpeedMaximum);

    // Note: Skipping pins 0x12 and 0x13 for audio usage
    unsigned dpi24pins[] =
    {
        0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    };

    // put all the pins in AltMode2 for the GPI case
    for (const unsigned pin : dpi24pins)
    {
        CGPIOPin(pin, GPIOModeAlternateFunction2);
    }

    // put audio pins into AltMode5 (GPI Case)
    CGPIOPin(0x12, GPIOModeAlternateFunction5);
    CGPIOPin(0x13, GPIOModeAlternateFunction5);

    // initialize interrupts
    _interruptSystem.Initialize();

    // initialize frame buffer
    _frameBuffer = new CBcmFrameBuffer(ScreenWidth, ScreenHeight, 16, ScreenWidth, ScreenHeight * 2);
    _frameBuffer->Initialize();
    memset((u16 *)_frameBuffer->GetBuffer(), 0xFF, _frameBuffer->GetSize());

    // setup page flipping within frame buffer
    _activePixelBuffer = (u16 *)_frameBuffer->GetBuffer();
    _frameBuffer->SetVirtualOffset(0, ScreenHeight);
    _videoPage = 0;

    // initialize serial
    _serialDevice.Initialize(115200);

    // use null device for logging
    _logger.Initialize(&_nullDevice);

    // initialize timer
    _timer.Initialize();

    // initialize EMMC
    _emmcDevice.Initialize();

    // mount SD card as FAT
    f_mount(&_fileSystem, _partitionName, 1);

    // initialze USB Host Controller Interface
    _usbHciDevice.Initialize();

    // initialize newlib stdio for file system access
    CGlueStdioInit(_fileSystem);

    CUSBGamePadDevice *gamePad = (CUSBGamePadDevice *)_deviceNameService.GetDevice("upad1", false /*bBlockDevice*/);
    gamePad->RegisterStatusHandler(GamePadStatusHandler);

    return true;
}

bool CircleKernel::IsButtonPressed(HostButton button)
{
    return (_buttonState & button) != 0;
}

void CircleKernel::StartSoundQueue()
{
    _pwmSoundDevice.AllocateQueue(SoundQueueSize);
    _pwmSoundDevice.SetWriteFormat(TSoundFormat::SoundFormatSigned16, 2 /*nChannels*/);

    constexpr size_t BytesPerFrame = 1000 * 2 * sizeof(s16);

    // prefill queue with silence
    unsigned int queueSizeFrames = _pwmSoundDevice.GetQueueSizeFrames();
    u8 zeroBuffer[BytesPerFrame] = {};
    for (unsigned int i = 0; i < queueSizeFrames; i++)
    {
        _pwmSoundDevice.Write(zeroBuffer,BytesPerFrame);
    }

    _pwmSoundDevice.Start();
}

HostExitCode CircleKernel::RunApp(int argc, const char *argv[])
{
    bool running = true;
    bool menuPressed = false;

    _menuEnable = false;
    OpenRomMenu menu(this);

    _gameBoy.reset(new GameBoy(GameBoyModel::Auto, "tetris.gb", this));

    StartSoundQueue();

    _powerEnablePin.Write(HIGH);

    int powerPressedFrames = 0;

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

        if (_powerButtonPin.Read() == LOW)
        {
            powerPressedFrames++;

            if (powerPressedFrames > 10)
            {
                running = false;
            }
        }
        else
        {
            powerPressedFrames = 0;
        }

        if (!menuPressed & IsButtonPressed(HostButton::Menu))
        {
            _menuEnable = !_menuEnable;
        }
        menuPressed = IsButtonPressed(HostButton::Menu);
    }

    _gameBoy.reset();

    // unmount SD card
    f_mount(0, "", 0);

    _powerEnablePin.Write(LOW); // powering down

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
    _buttonState |= (pState->buttons & GamePadButtonSelect) && (pState->buttons & GamePadButtonY) ? HostButton::Menu : HostButton::None;
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

void CircleKernel::LoadRomFile(const char *romFile)
{
    _gameBoy.reset(new GameBoy(GameBoyModel::Auto, romFile, this));
    _menuEnable = false;
}

void CircleKernel::QueueAudio(s16 *buffer, u32 sampleCount)
{
    size_t bytesToWrite = sampleCount * 2 * sizeof(s16);
    _pwmSoundDevice.Write(buffer, bytesToWrite);
}

void CircleKernel::SyncAudio()
{
    // keep the queue around half full
    unsigned maxFrames = _pwmSoundDevice.GetQueueSizeFrames() / 2;

    while (_pwmSoundDevice.GetQueueFramesAvail() > maxFrames)
    {
    }
}

void CircleKernel::PushVideoFrame(u32 *pixelBuffer)
{
    u32 pitch = _frameBuffer->GetPitch() / sizeof(TScreenColor);
    u32 width = _frameBuffer->GetWidth();
    u32 height = _frameBuffer->GetHeight();

    int cx = width > 160 ? width / 2 - 160 / 2 : 0;
    int cy = height > 144 ? height / 2 - 144 / 2 : 0;

    for (int x = 0; x < 160; x++)
    for (int y = 0; y < 144; y++)
    {
        u32 gbColor = pixelBuffer[y * 160 + x];
        u8 r = gbColor >> 24;
        u8 g = gbColor >> 16;
        u8 b = gbColor >> 8;

        _activePixelBuffer[(x + cx) + ((y + cy) * pitch)] = COLOR16(r >> 3, g >> 3, b >> 3);
    }

    PresentPixelBuffer();
}

void CircleKernel::PresentPixelBuffer()
{
    // wait for v-sync
    _frameBuffer->WaitForVerticalSync();

    // present current page to screen
    _frameBuffer->SetVirtualOffset(0, ScreenHeight * _videoPage);

    // move to next page
    _videoPage = (_videoPage + 1) & 1;

    // get address of new video page
    _activePixelBuffer = ((u16 *)_frameBuffer->GetBuffer() + (ScreenHeight * ScreenWidth * _videoPage));
}