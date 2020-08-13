#include "Kernel.h"
#include "GameBoyCart.h"
#if USE_SDL
#else
#include <circle/usb/usbgamepad.h>
#include <circle/string.h>
#include <circle/util.h>
#endif
#include <assert.h>

// GPI internal screen res
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

static const char FromKernel[] = "kernel";

// controller buttons
static bool m_pressedDown;
static bool m_pressedUp;
static bool m_pressedLeft;
static bool m_pressedRight;

Kernel::Kernel()
:
#if USE_SDL
    _window(nullptr),
    _surface(nullptr),
#else
    _screen(SCREEN_WIDTH, SCREEN_HEIGHT),
    _timer(&_interrupt),
    _logger(_options.GetLogLevel(), &_timer),
    _usbhci(&_interrupt, &_timer),
#endif
    _posX(SCREEN_WIDTH / 2),
    _posY(SCREEN_HEIGHT / 2)
{
#if !USE_SDL
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
#endif
}

Kernel::~Kernel()
{
}

bool Kernel::Initialize()
{
    bool ok = true;

#ifdef USE_SDL

    SDL_Init(SDL_INIT_VIDEO);

    _window = SDL_CreateWindow(
        "BearGB",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0);
    _surface = SDL_GetWindowSurface(_window);

#else
    ok = ok && _screen.Initialize();
    ok = ok && _serial.Initialize(115200);

    CDevice *logTarget = _deviceNameService.GetDevice(_options.GetLogDevice(), false /*hBlockDevice*/);
    if (logTarget == 0)
    {
        logTarget = &_screen;
    }
    ok = ok && _logger.Initialize(logTarget);

    ok = ok && _interrupt.Initialize();
    ok = ok && _timer.Initialize();
    ok = ok && _usbhci.Initialize();
#endif

    _gameBoy.reset(new GameBoy(GameBoyModel::GameBoy, "tetris.gb"));

    return ok;
}

ShutdownMode Kernel::Run()
{
    bool foundGamePad = false;

#if USE_SDL
    SDL_Event event;
#else
    for (unsigned deviceIdx = 1; 1; deviceIdx++)
    {
        CString deviceName;
        deviceName.Format ("upad%u", deviceIdx);

        CUSBGamePadDevice *gamePad =
            (CUSBGamePadDevice *)_deviceNameService.GetDevice(deviceName, false /*bBlockDevice*/);
        if (gamePad == 0)
        {
            break;
        }

        const TGamePadState *pState = gamePad->GetInitialState();
        assert (pState != 0);

        gamePad->RegisterStatusHandler(GamePadStatusHandler);

        foundGamePad = TRUE;
    }

    if (!foundGamePad)
    {
        _logger.Write(FromKernel, LogPanic, "Gamepad not found");
    }

    CBcmFrameBuffer *frameBuffer = _screen.GetFrameBuffer();
#endif

    bool running = true;

    while (running)
    {
        _gameBoy->RunOneFrame();

#if USE_SDL
        SDL_LockSurface(_surface);
        //memset(_surface->pixels, 0xFF, _surface->h * _surface->pitch);

        for (int x = 0; x < _surface->w; x++)
        for (int y = 0; y < _surface->h; y++)
        {
            ((u8*)_surface->pixels)[(y * _surface->w + x) * 4 + 0] = y & 0xFF; // blue
            ((u8*)_surface->pixels)[(y * _surface->w + x) * 4 + 1] = x / 2 & 0xFF; // green
            ((u8*)_surface->pixels)[(y * _surface->w + x) * 4 + 2] = 0xFF - y & 0xFF; // red
            ((u8*)_surface->pixels)[(y * _surface->w + x) * 4 + 3] = 0; // alpha
        }

        SDL_UnlockSurface(_surface);

        SDL_UpdateWindowSurface(_window);

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    running = false;
            }
        }
#else
        frameBuffer->WaitForVerticalSync();

        // clear frame buffer
        memset((u32 *) (uintptr)frameBuffer->GetBuffer(), 0, frameBuffer->GetSize());
#endif

        if (m_pressedDown)
        {
            _posY++;
        }
        else if (m_pressedUp)
        {
            _posY--;
        }

        if (m_pressedRight)
        {
            _posX++;
        }
        else if (m_pressedLeft)
        {
            _posX--;
        }

		const unsigned size = 16;

		// draw cross on screen
		for (unsigned pos = 0; pos < size; pos++)
		{
			unsigned nPosX = _posX + pos;
			unsigned nPosY = _posY + pos;

#if !USE_SDL
			_screen.SetPixel(nPosX, nPosY, NORMAL_COLOR);
			_screen.SetPixel((_posX + size)-pos-1, nPosY, NORMAL_COLOR);
#endif
		}
    }

#if USE_SDL
    SDL_DestroyWindow(_window);
    SDL_Quit();
#endif

    return ShutdownHalt;
}

#if !USE_SDL
void Kernel::GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState)
{
	m_pressedUp = (pState->buttons & GamePadButtonUp) == GamePadButtonUp;
	m_pressedDown = (pState->buttons & GamePadButtonDown) == GamePadButtonDown;
	m_pressedLeft = (pState->buttons & GamePadButtonLeft) == GamePadButtonLeft;
	m_pressedRight = (pState->buttons & GamePadButtonRight) == GamePadButtonRight;
}
#endif