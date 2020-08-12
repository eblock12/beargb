#include "kernel.h"
#include <circle/usb/usbgamepad.h>
#include <circle/string.h>
#include <circle/util.h>
#include <assert.h>

static const char FromKernel[] = "kernel";

// controller buttons
static bool m_pressedDown;
static bool m_pressedUp;
static bool m_pressedLeft;
static bool m_pressedRight;

Kernel::Kernel()
:   _screen(320, 240), // GPI Screen res
    _timer(&_interrupt),
    _logger(_options.GetLogLevel(), &_timer),
    _usbhci(&_interrupt, &_timer),
    _posX(160),
    _posY(120)
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
}

Kernel::~Kernel()
{
}

bool Kernel::Initialize()
{
    bool ok = true;

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

    return ok;
}

ShutdownMode Kernel::Run()
{
    bool foundGamePad = false;

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

    while (true)
    {
        frameBuffer->WaitForVerticalSync();

        // clear frame buffer
        memset((u32 *) (uintptr)frameBuffer->GetBuffer(), 0, frameBuffer->GetSize());

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

			_screen.SetPixel(nPosX, nPosY, NORMAL_COLOR);
			_screen.SetPixel((_posX + size)-pos-1, nPosY, NORMAL_COLOR);
		}
    }

    return ShutdownHalt;
}


void Kernel::GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState)
{
	m_pressedUp = (pState->buttons & GamePadButtonUp) == GamePadButtonUp;
	m_pressedDown = (pState->buttons & GamePadButtonDown) == GamePadButtonDown;
	m_pressedLeft = (pState->buttons & GamePadButtonLeft) == GamePadButtonLeft;
	m_pressedRight = (pState->buttons & GamePadButtonRight) == GamePadButtonRight;

	//CString Value;

	//CLogger::Get ()->Write (FromKernel, LogNotice, Msg);
}