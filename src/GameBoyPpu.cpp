#include "GameBoyPpu.h"
#include "GameBoy.h"

#include <memory.h>
#include <iostream>

GameBoyPpu::GameBoyPpu(GameBoy *gameBoy, u8 *videoRam, u8 *oamRam)
{
    _gameBoy = gameBoy;
    _videoRam = videoRam;
    _oamRam = oamRam;

    _pixelBuffer = new u32[160 * 144];
    memset(_pixelBuffer, 0, 160 * 144 * sizeof(u32));
}

GameBoyPpu::~GameBoyPpu()
{
    delete[] _pixelBuffer;
}

void GameBoyPpu::ExecuteCycle()
{
    // Timing reference: https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf

    if (_state.lcdPower)
    {
        if (_state.scanline < 144)
        {
            // currently drawing a visible scanline

            if (_pixelsRendered == 160)
            {
                _state.lcdStatus &= ~0x03;
                _pixelsRendered = 0;
            }

            // handle events that occur at each cycle within scanline
            switch (_state.tick)
            {
                case 0:
                    _state.lcdCurrentScanline = _state.scanline;
                    break;
                case 3:
                    if (_state.scanline == 0)
                    {
                        _state.lcdStatus &= 0x3; // H-Blank for one cycle for scanline 0
                    }
                    break;
                case 4:
                    _state.lcdStatus |= 0x2; // OAM search mode
                    _state.lcdStatus &= 0x1;
                    break;
                case 84:
                    _state.lcdStatus |= 0x3; // Drawing mode
                    StartRender();
                    break;
                case 456: // end of scanline
                    _state.tick = 0;
                    _state.scanline++;
                    if (_state.scanline == 144)
                    {
                        // entering v-blank, LY is updated immediately
                        _state.lcdCurrentScanline = _state.scanline;
                    }
                    break;
            }
        }
        else
        {
            // currently in v-blank
            switch (_state.tick)
            {
                case 4:
                    if (_state.scanline < 153)
                    {
                        _state.lcdStatus &= 0x2;
                        _state.lcdStatus |= 0x1; // v-blank mode
                        _gameBoy->SetInterruptFlags(IrqFlag::VBlank);
                    }
                    break;
                case 12:
                    if (_state.scanline == 153)
                    {
                        // on last line of v-blank this goes to 0 early
                        _state.lcdCurrentScanline = 0;
                    }
                    break;
                case 456:
                    _state.tick = 0;
                    _state.scanline++;
                    if (_state.scanline == 154)
                    {
                        // exiting v-blank, starting new frame
                        _state.scanline = 0;
                    }
                    else
                    {
                        _state.lcdCurrentScanline = _state.scanline;
                    }
                    break;
            }
        }

        if ((_state.lcdStatus & 0x03) == 0x03)
        {
            // in drawing mode
            TickDrawing();

            if (_pixelsRendered == 160)
            {
                // enter v-blank
                _state.lcdStatus &= ~(0x03);
            }
        }

        _state.tick++;
    }
}

void GameBoyPpu::TickBgFetcher()
{
    u16 tileMapAddr, tileSetAddr;
    u8 y, tileRow, tileIndex;


    switch (_fetcherBg.tick)
    {
        case 1: // fetch tile
            // TODO: Use window tile map if inside a window

            tileMapAddr = (_state.lcdControl & 0x08) ? 0x1C00 : 0x1800; // base offset into VRAM
            y = _state.scrollY + _state.scanline;

            tileRow = y / 8;
            tileMapAddr += _bgColumn + (tileRow * 32);
            tileIndex = _videoRam[tileMapAddr];

            // TODO: Fetch attributes for CGB, extended tile banks

            tileSetAddr = (_state.lcdControl & 0x10) ? 0x0000 : 0x1000;
            tileSetAddr += tileIndex * 16 + (y & 0x07) * 2; // 2 bytes per row, 16 bytes per tile
            _fetcherBg.tileSetAddr = tileSetAddr;
            break;

        case 3: // fetch first tile data byte
            _fetcherBg.tileData0 = _videoRam[_fetcherBg.tileSetAddr];
            break;

        case 5: // fetch second tile data byte
            _fetcherBg.tileData1 = _videoRam[_fetcherBg.tileSetAddr + 1];
            // fall through

        case 6:
        case 7:
            if (_fifoBg.length == 0) // is FIFO ready for new data?
            {
               // std::cout << "FIFO: ";

                for (int i = 0; i < 8; i++)
                {
                    _fifoBg.data[i] =
                        ((_fetcherBg.tileData0 >> (7 - i)) & 0x01) |
                        (((_fetcherBg.tileData1 >> (7 - i)) & 0x01) << 1);
                    //std::cout << std::hex << int(_fifoBg.data[i]) << ' ';
                }
               // std::cout << std::endl;

                _bgColumn = (_bgColumn + 1) & 0x1F;
                _fifoBg.position = 0;
                _fifoBg.length = 8;
                _fetcherBg.tick = 0;
            }
            else
            {
                _fetcherBg.tick = 6; // hold here until FIFO is ready
            }
            break;
    }

    _fetcherBg.tick++;
}

void GameBoyPpu::TickDrawing()
{
    if (_fifoBg.length > 0)
    {
        // have entered visible area?
        if (_pixelsRendered >= 0)
        {
            u16 bufferOffset = _state.scanline * 160 + _pixelsRendered;
            if (bufferOffset < 0 || bufferOffset >= 23040)
            {
                std::cout << "PIXEL BUFFER OUT-OF-RANGE! " << std::dec << int(_pixelsRendered) << "x" << int(_state.scanline) << std::endl;
            }
            else
            {
                u8 bgColorIndex = _fifoBg.data[_fifoBg.position];
                u8 color = (_state.bgPal >> (bgColorIndex * 2)) & 0x03;
                //std::cout << "Write color: " << int(color) << std::endl;
                _pixelBuffer[bufferOffset] = _gbPal[color];
            }
        }

        _fifoBg.Pop();
        _pixelsRendered++;
    }

    TickBgFetcher();
}

u8 GameBoyPpu::ReadRegister(u16 addr)
{
    switch (addr)
    {
        case 0xFF40: // LCD Control
            return _state.lcdControl;
        case 0xFF41: // STAT - LCD Status
            return _state.lcdStatus | 0x80; // upper bit is always 1
        case 0xFF42: // SCY - BG Scroll Y
            return _state.scrollY;
        case 0xFF43: // SCX - BG Scroll X
            return _state.scrollX;
        case 0xFF44: // LY - LCD Current Scanline
            return _state.lcdCurrentScanline;
        case 0xFF47: // BGP
            return _state.bgPal;
        case 0xFF48: // OBP0
            return _state.objPal0;
        case 0xFF49: // OBP1
            return _state.objPal1;
        case 0xFF4A: // WY
            return _state.windowY;
        case 0xFF4B: // WX
            return _state.windowX;
    }

    std::cout << "Read from unmapped PPU register, addr=" << std::hex << int(addr) << std::endl;
    return 0xFF;
}

void GameBoyPpu::Reset()
{
}

void GameBoyPpu::StartRender()
{
    _fifoOam.Clear();
    _fifoBg.Clear();
    _fifoBg.length = 8;

    _fetcherOam.tick = 0;
    _fetcherBg.tick = 0;

    // rendering starts off screen from -8 to -16 pixels
    _pixelsRendered = -8 - (_state.scrollX & 0x07);
    _bgColumn = _state.scrollX / 8;
}

void GameBoyPpu::SetLcdPower(bool enable)
{
    _state.lcdPower = enable;

    if (_state.lcdPower) // powering on
    {
        _state.tick = 0;
    }
    else // powering off
    {
        _state.tick = 0;
        _state.scanline = 0;
        _state.lcdCurrentScanline = 0;
        _state.lcdStatus &= 0xFC; // clear mode
    }
}

void GameBoyPpu::WriteRegister(u16 addr, u8 val)
{
    switch (addr)
    {
        case 0xFF40:
            _state.lcdControl = val;
            if (_state.lcdPower != ((_state.lcdControl & 0x80) != 0))
            {
                // LCD power has been toggled
                SetLcdPower((_state.lcdControl & 0x80) != 0);
            }
            return;
        case 0xFF41: // STAT - LCD Status
            _state.lcdStatus = val & 0xF8; // lower 3 bits are read-only
            // TODO: Check if interrupts need to be raised after STAT change
            return;
        case 0xFF42: // SCY - BG Scroll Y
            _state.scrollY = val;
            return;
        case 0xFF43: // SCX - BG Scroll X
            _state.scrollX = val;
            return;
        case 0xFF47: // BGP
            _state.bgPal = val;
            return;
        case 0xFF48: // OBP0
            _state.objPal0 = val;
            return;
        case 0xFF49: // OBP1
            _state.objPal1 = val;
            return;
        case 0xFF4A: // WY
            _state.windowY = val;
            return;
        case 0xFF4B: // WX
            _state.windowX = val;
            return;
    }

    std::cout << "Write to unmapped PPU register, addr=" << std::hex << int(addr) << std::endl;
}

u8 GameBoyPpu::ReadVideoRam(u16 addr)
{
    if ((_state.lcdStatus & 0x03) == 0x03)
    {
        // VRAM is disallowed in mode 3
        std::cout << "Warning! Disallowed VRAM read at addr " << std::hex << int(addr) << std::endl;
        return 0xFF;
    }
    else
    {
        // TODO: Allow accessing high CGB video RAM bank
        return _videoRam[addr & 0x1FFF];
    }
}

void GameBoyPpu::WriteVideoRam(u16 addr, u8 val)
{
    if ((_state.lcdStatus & 0x03) == 0x03)
    {
        // VRAM is disallowed in mode 3
        std::cout << "Warning! Disallowed VRAM write at addr " << std::hex << int(addr) << std::endl;
    }
    else
    {
        // TODO: Allow accessing high CGB video RAM bank
        _videoRam[addr & 0x1FFF] = val;
    }
}

u8 GameBoyPpu::ReadOamRam(u8 addr)
{
    if ((addr < 160) && (((_state.lcdStatus & 0x03) <= 1))) // if in DMA or V-Blank or H-Blank, reads are allowed
    {
        return _oamRam[addr];
    }
    else
    {
        std::cout << "Warning! Disallowed OAM read at addr " << std::hex << int(addr) << std::endl;
    }
}

void GameBoyPpu::WriteOamRam(u8 addr, u8 val, bool dmaBypass)
{
    if ((addr < 160) && (dmaBypass || ((_state.lcdStatus & 0x03) <= 1))) // if in DMA or V-Blank or H-Blank, writes are allowed
    {
        _oamRam[addr] = val;
    }
    else
    {
        std::cout << "Warning! Disallowed OAM read at addr " << std::hex << int(addr) << std::endl;
    }
}