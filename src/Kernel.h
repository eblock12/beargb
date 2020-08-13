#pragma once

#if USE_SDL
#include <SDL.h>
#include "shared.h"
#else
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
#endif

#include <memory>
#include "GameBoy.h"

enum ShutdownMode
{
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot,
};

class Kernel
{
private:
#if USE_SDL
    SDL_Window *_window;
    SDL_Surface *_surface;
#else
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
#endif

    std::unique_ptr<GameBoy> _gameBoy;

	// character position
	int _posX;
	int _posY;
public:
	Kernel();
	~Kernel();

	bool Initialize();

	ShutdownMode Run();
};