#pragma once

#include <circle/memory.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbgamepad.h>
#include <circle/types.h>

enum ShutdownMode
{
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot,
};

class Kernel
{
private:
    CMemorySystem _memory;
    CActLED _actLED;
    CKernelOptions _options;
    CDeviceNameService _deviceNameService;
    CScreenDevice _screen;
    CSerialDevice _serial;
    CExceptionHandler _exceptionHandler;
    CInterruptSystem _interrupt;
    CTimer _timer;
    CLogger _logger;
    CUSBHCIDevice _usbhci;

    static void GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState);

	// character position
	int _posX;
	int _posY;
public:
	Kernel();
	~Kernel();

	boolean Initialize();

	ShutdownMode Run();
};