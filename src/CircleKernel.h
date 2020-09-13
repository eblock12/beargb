#pragma once

#include "GameBoy.h"
#include "IHostSystem.h"
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/nulldevice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/cputhrottle.h>
#include <circle/serial.h>
#include <circle/bcmframebuffer.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbgamepad.h>
#include <circle/pwmsoundbasedevice.h>
#include <SDCard/emmc.h>
#include <circle_glue.h>
#include <queue>
#include <memory>

enum TShutdownMode
{
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
};

typedef u16 TScreenColor;

class CircleKernel : public IHostSystem
{
private:
    char const *_kernelName;
    char const *_partitionName;
    CActLED _actLED;
    CKernelOptions _kernelOptions;
    CDeviceNameService _deviceNameService;
    CNullDevice _nullDevice;
    CExceptionHandler _exceptionHandler;
    CInterruptSystem _interruptSystem;
    CSerialDevice _serialDevice;
    CTimer _timer;
    CLogger _logger;
    CUSBHCIDevice _usbHciDevice;
    CEMMCDevice _emmcDevice;
    FATFS _fileSystem;
    CBcmFrameBuffer *_frameBuffer;
    CCPUThrottle _cpuThrottle;
    CPWMSoundBaseDevice _pwmSoundDevice;

    u16 *_activePixelBuffer;
    u8 _videoPage;

    static u16 _buttonState;

    std::unique_ptr<GameBoy> _gameBoy;

    void StartSoundQueue();

    // unused
    TShutdownMode Run() { return TShutdownMode::ShutdownHalt; }

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
    virtual u16 *GetPixelBuffer() override { return _activePixelBuffer; }
    virtual void PresentPixelBuffer() override;
};