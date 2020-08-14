#include "GameBoyPpu.h"
#include "GameBoy.h"

#include <iostream>

GameBoyPpu::GameBoyPpu(GameBoy *gameBoy, u8 *videoRam, u8 *oamRam)
{
    _gameBoy = gameBoy;
    _videoRam = videoRam;
    _oamRam = oamRam;
}

GameBoyPpu::~GameBoyPpu()
{
}

void GameBoyPpu::ExecuteCycle()
{
    // Timing reference: https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf

    if (_state.lcdPower)
    {
        if (_state.scanline < 144)
        {
            // currently drawing a visible scanline

            // handle events that occur at each cycle within scanline
            switch (_state.tick)
            {
                case 0:
                    _state.lcdCurrentScanline = _state.scanline;
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
                case 12:
                    if (_state.scanline == 153)
                    {
                        // on last line of v-blank this goes to 0 early
                        _state.lcdCurrentScanline = 0;
                    }
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

        _state.tick++;
    }
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