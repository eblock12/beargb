#include "GameBoyPpu.h"
#include "GameBoy.h"

#include <memory.h>
#include <iostream>

//#define TRACE

GameBoyPpu::GameBoyPpu(GameBoy *gameBoy, IHostSystem *host, u8 *videoRam, u8 *oamRam)
{
    _gameBoy = gameBoy;
    _host = host;
    _videoRam = videoRam;
    _oamRam = oamRam;

    _state.scanline = 0;
    _pixelsRendered = 0;

    // build CGB color lookup
    // Found at https://byuu.net/video/color-emulation/
    // Original author is unknown
    for (u32 r = 0; r < 32; r++)
    for (u32 g = 0; g < 32; g++)
    for (u32 b = 0; b < 32; b++)
    {
        u8 r2 = std::min<u32>((r * 26 + g *  4 + b *  2), 960) >> 2;
        u8 g2 = std::min<u32>((         g * 24 + b *  8), 960) >> 2;
        u8 b2 = std::min<u32>((r *  6 + g *  4 + b * 22), 960) >> 2;

        _cgbPal[r][g][b] = COLOR16(r2 >> 3, g2 >> 3, b2 >> 3);
    }

}

GameBoyPpu::~GameBoyPpu()
{
}

void GameBoyPpu::ExecuteCycle()
{
    // Timing reference: https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf

    u8 preserveMode = _state.lcdMode;

    if (!_state.lcdPower)
    {
        if ((_gameBoy->GetCycleCount() % (154 * 456)) == 0)
        {
            _host->PresentPixelBuffer();
            //_host->SyncAudio();
        }
        return;
    }

    _state.tick++;
    if (_state.sleepCycles > 0)
    {
        _state.sleepCycles--;
        return;
    }

    if (_state.scanline < 144)
    {
        // currently drawing a visible scanline

        // h-blank doesnt occur at a fixed cycle count
        // time to finish rendering varies
        if (_pixelsRendered == 160)
        {
            _state.lcdMode = LcdModeFlag::HBlank;
            _pixelsRendered = 0;
            _state.sleepCycles = 456 - _state.tick - 1; // sleep through H-Blank
        }

        // handle events that occur at each cycle within scanline
        switch (_state.tick)
        {
            case 3:
                _state.ly = _state.scanline;
                break;
            case 4:
                _state.lcdMode = LcdModeFlag::OamSearch;
                _spritesFound = 0;
                break;
            case 84:
                _state.lcdMode = LcdModeFlag::Drawing;
                StartRender();
                _renderPaused = true;
                break;
            case 89:
                _renderPaused = false;
                break;
            case 456: // end of scanline
                _state.sleepCycles = 0;
                _state.tick = 0;
                _state.scanline++;
                if (_state.scanline == 144)
                {
                    // entering v-blank, LY is updated immediately
                    _state.ly = _state.scanline;
                }
                break;
        }
    }
    else
    {
        // currently in v-blank
        switch (_state.tick) // cycle count within scanline
        {
            case 4:
                if (_state.scanline == 144)
                {
                    _state.lcdMode = LcdModeFlag::VBlank;
                    _windowOffset = 0;
                    _gameBoy->SetInterruptFlags(IrqFlag::VBlank);
                    _host->PresentPixelBuffer();
                    //_host->SyncAudio();
                    _gameBoy->CheckJoyPadChange();
                }
                break;
            case 12:
                if (_state.scanline == 153)
                {
                    // on last line of v-blank this goes to 0 early
                    _state.ly = 0;
                }
                _state.sleepCycles = 443; // sleep until end of scanline
                break;
            case 456: // end of v-blank line
                _state.tick = 0;
                _state.scanline++;
                if (_state.scanline == 154)
                {
                    // exiting v-blank, starting new frame
                    _state.scanline = 0;
                    _state.ly = 0;
                }
                else
                {
                    _state.ly = _state.scanline;
                }
                break;
        }
    }

    if (_state.lcdMode == LcdModeFlag::Drawing)
    {
        if (!_renderPaused)
        {
            // in drawing mode
            TickDrawing();
        }

        if (_pixelsRendered == 160)
        {
            // enter h-blank
            _state.lcdMode = LcdModeFlag::HBlank;

            if (_state.scanline < 143)
            {
                _gameBoy->ExecuteCgbHdma();
            }
        }
    }
    else if (_state.lcdMode == LcdModeFlag::OamSearch)
    {
        TickOamSearch();
    }

    if ((preserveMode != _state.lcdMode) ||
        (_state.lyCoincident != (_state.ly == _state.lyCompare)))
    {
        _state.lyCoincident = (_state.ly == _state.lyCompare);
        CheckLcdStatusIrq();
    }
}

void GameBoyPpu::TickBgFetcher()
{
    u16 tileMapAddr, tileSetAddr;
    u8 y, tileRow, tileIndex, tileAttributes, tileY;


    switch (_fetcherBg.tick++)
    {
        case 1: // fetch tile
            if (_insideWindow)
            {
                tileMapAddr = (_state.lcdControl & 0x40) ? 0x1C00 : 0x1800;
                y = (u8)_windowOffset - 1;
            }
            else
            {
                tileMapAddr = (_state.lcdControl & 0x08) ? 0x1C00 : 0x1800; // base offset into VRAM
                y = _state.scrollY + _state.scanline;
            }

            // fetch tile index and CGB attributes
            tileRow = y / 8;
            tileMapAddr += _bgColumn + (tileRow * 32);
            tileIndex = _videoRam[tileMapAddr];
            tileAttributes = _gameBoy->IsCgb() ? _videoRam[0x2000 | tileMapAddr] : 0;

            // calculate tile set address
            y &= 0x07;
            tileY = (tileAttributes & 0x40) ? (7 - y) : y; // flip vertically
            tileSetAddr = (_state.lcdControl & 0x10) ? 0x0000 : 0x1000;
            tileSetAddr += (tileSetAddr ?
                (s8)tileIndex * 16 :
                tileIndex * 16) + tileY * 2; // 2 bytes per row, 16 bytes per tile
            tileSetAddr |= (tileAttributes & 0x08) ? 0x2000 : 0x0000;

            _fetcherBg.tileSetAddr = tileSetAddr;
            _fetcherBg.attributes = tileAttributes;
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
                for (int i = 0; i < 8; i++)
                {
                    u8 x = (_fetcherBg.attributes & 0x20) ? i : (7 - i);
                    _fifoBg.data[i].color =
                        ((_fetcherBg.tileData0 >> x) & 0x01) |
                        (((_fetcherBg.tileData1 >> x) & 0x01) << 1);
                    _fifoBg.data[i].attributes = _fetcherBg.attributes;
                }

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
}

void GameBoyPpu::TickDrawing()
{
    // did drawing transition over the BG/window boundary?
    if (_windowEnable &&
        !_insideWindow &&
        (_pixelsRendered >= _windowStartX - 7) &&
        (_state.scanline >= _windowStartY))
    {
        _insideWindow = true;

        _windowOffset++;

        //std::cout << "X=" << std::dec << int(_pixelsRendered) << " Y=" << int(_state.scanline) << " winY=" << int(_windowOffset) << std::endl;

        // reset fetch/fifo
        _bgColumn = 0;
        _fetcherBg.tick = 0;
        _fifoBg.Clear();

        // bail out, nothing will happen on first cycle of transition
        return;
    }

    if (_fetchNextSprite && (_state.lcdControl & 0x02))
    {
        MoveToNextSprite();
    }
    if (!_fetchNextSprite)
    {
        TickOamFetcher();
        if (_fetchNextSprite && (_state.lcdControl & 0x02))
        {
            MoveToNextSprite();
        }
    }

    if (_fifoBg.length > 0 && _fetchNextSprite)
    {
        // have entered visible area?
        if (_pixelsRendered >= 0)
        {
            u16 bufferOffset = _state.scanline * 160 + _pixelsRendered;
            u8 bgColorIndex = _fifoBg.data[_fifoBg.position].color;
            u8 bgAttributes = _fifoBg.data[_fifoBg.position].attributes;
            u8 spriteColorIndex = _fifoOam.data[_fifoOam.position].color;
            u8 spriteAttributes = _fifoOam.data[_fifoOam.position].attributes;

            u16 *pixelBuffer = _host->GetPixelBuffer();

            // check if sprite or BG pixel has priority
            if ((spriteColorIndex != 0) && // sprite pixel is opaque,
                ((bgColorIndex == 0) || // BG pixel is transparent
                ((spriteAttributes & 0x80) == 0x0 && (bgAttributes & 0x80) == 0))) // sprite has priority and BG doesn't have priority
            {
                // Sprite pixel has priority

                if (_gameBoy->IsCgb())
                {
                    CgbPalEntry color = _state.cgbObjPal[spriteColorIndex | ((spriteAttributes & 0x07) << 2)];
                    pixelBuffer[bufferOffset] = _cgbPal[color.r][color.g][color.b];
                }
                else
                {
                    u8 palette = (spriteAttributes & 0x10) != 0 ? _state.objPal1 : _state.objPal0;
                    u8 color = (palette >> (spriteColorIndex * 2)) & 0x03;
                    pixelBuffer[bufferOffset] = _dmgPal[color];
                }
            }
            else
            {
                if (_gameBoy->IsCgb())
                {
                    CgbPalEntry color = _state.cgbBgPal[bgColorIndex | ((bgAttributes & 0x07) << 2)];
                    pixelBuffer[bufferOffset] = _cgbPal[color.r][color.g][color.b];
                }
                else
                {
                    u8 color = (_state.bgPal >> (bgColorIndex * 2)) & 0x03;
                    pixelBuffer[bufferOffset] = _dmgPal[color];
                }
            }
        }

        _fifoBg.Pop();
        if (_fifoOam.length > 0)
        {
            _fifoOam.Pop();
        }
        _pixelsRendered++;
    }

    TickBgFetcher();
}

void GameBoyPpu::TickOamFetcher()
{
    s16 spriteY;
    u8 spriteTileIndex;
    u8 spriteAttribute;
    u8 spriteRow;

    switch (_fetcherOam.tick++)
    {
        case 1: // fetch sprite tile
            spriteY = (s16)_oamRam[_fetchOamAddr] - 16;
            spriteTileIndex = _oamRam[_fetchOamAddr + 2];
            spriteAttribute = _oamRam[_fetchOamAddr + 3];

            if (_state.lcdControl & 0x04)
            {
                // large sprites enabled (8x16)

                spriteTileIndex &= 0xFE; // second half of sprite will use index+1

                if ((spriteAttribute & 0x40) != 0)
                {
                    // Y mirroring
                    spriteRow = 15 - (_state.scanline - spriteY);
                }
                else
                {
                    spriteRow = _state.scanline - spriteY;
                }
            }
            else
            {
                // use normal sized sprites

                if ((spriteAttribute & 0x40) != 0)
                {
                    // Y mirroring
                    spriteRow = 7 - (_state.scanline - spriteY);
                }
                else
                {
                    spriteRow = _state.scanline - spriteY;
                }
            }

            _fetcherOam.tileSetAddr = (spriteTileIndex * 16) + (spriteRow * 2);
            if (_gameBoy->IsCgb())
            {
                _fetcherOam.tileSetAddr += (spriteAttribute & 0x08) ? 0x2000 : 0x0000;
            }
            _fetcherOam.attributes = spriteAttribute;
            break;

        case 3: // fetch first tile data byte
            _fetcherOam.tileData0 = _videoRam[_fetcherOam.tileSetAddr];
            break;

        case 5: // fetch second tile data byte
            _fetcherOam.tileData1 = _videoRam[_fetcherOam.tileSetAddr+1];
            _fetchNextSprite = true;
            _fetcherOam.tick = 0;

            if ((_state.lcdControl & 0x02) != 0) // sprite render enable?
            {
                for (int i = 0, j = _fifoOam.position; i < 8; i++, j = (j + 1) & 7) // fill pixel FIFO queue
                {
                    u8 x = (_fetcherOam.attributes & 0x20) != 0 ? i : (7 - i); // horizontal mirror
                    u8 color =
                        ((_fetcherOam.tileData0 >> x) & 0x01) |
                        (((_fetcherOam.tileData1 >> x) & 0x01) << 1);
                    if ((_fifoOam.data[j].color == 0) && color)
                    {
                        _fifoOam.data[j].color = color;
                        _fifoOam.data[j].attributes = _fetcherOam.attributes;
                    }
                }
                _fifoOam.length = 8;
            }
            break;
    }
}

void GameBoyPpu::TickOamSearch()
{
    // not sure what happens on first and second cycle exactly, just run on second cycle
    if ((_state.tick & 0x01) && // run every other tick
        (_spritesFound < 10)) // stop searching at 10 sprites
    {
        // search begins at cycle 4 in scanline
        // takes 2 cycles to evalulate each sprite
        // each sprite has 4 bytes in OAM RAM
        u8 oamAddr = ((_state.tick - 4) / 2) * 4;

        s16 oamY = (s16)_oamRam[oamAddr] - 16;

        //std::cout << "Oam search, addr=" << std::dec << int(oamAddr) << std::endl;

        // does sprite fall within this scanline? (sprite height may be 16 or 8)
        if ((_state.scanline >= oamY) &&
            (_state.scanline < oamY + ((_state.lcdControl & 0x04) ? 16 : 8)))
        {
            //std::cout << "found Sprite=" << int(_spritesFound) << " Y=" << int(oamY) << std::endl;

            // store results for tile fetcher in drawing phase
            _spriteX[_spritesFound] = _oamRam[oamAddr + 1];
            _spriteAddr[_spritesFound] = oamAddr;
            _spritesFound++;
        }
    }
}

void GameBoyPpu::MoveToNextSprite()
{
    // move to next search result from OAM search phase
    if (_fetchNextSprite && ((_state.lcdControl & 0x02) || _gameBoy->IsCgb()))
    {
        for (int i = 0; i < _spritesFound; i++)
        {
            if (_pixelsRendered == ((s16)_spriteX[i] - 8))
            {
                _fetchNextSprite = false;
                _fetchOamAddr = _spriteAddr[i];
                _spriteX[i] = 255; // remove from result
                _fetcherOam.tick = 0; // start fetcher

                break;
            }
        }
    }
}

void GameBoyPpu::CheckLcdStatusIrq()
{
    // check if any conditions are met where STAT irq should be raised
    bool triggerStat =
        _state.lcdPower &&
        (((_state.lcdStatus & LcdStatusFlags::CoincidentScanline) && _state.lyCoincident) ||
        ((_state.lcdStatus & LcdStatusFlags::OamSearch) && (_state.lcdMode == LcdModeFlag::OamSearch)) ||
        ((_state.lcdStatus & LcdStatusFlags::VBlank) && (_state.lcdMode == LcdModeFlag::VBlank)) ||
        ((_state.lcdStatus & LcdStatusFlags::HBlank) && (_state.lcdMode == LcdModeFlag::HBlank)));

    if (triggerStat && !_state.raisedStatIrq)
    {
        _gameBoy->SetInterruptFlags(IrqFlag::LcdStat);
    }
    _state.raisedStatIrq = triggerStat;
}

u8 GameBoyPpu::ReadRegister(u16 addr)
{
    switch (addr)
    {
        case 0xFF40: // LCD Control
            return _state.lcdControl;
        case 0xFF41: // STAT - LCD Status
            return (_state.lcdStatus & 0x78) |
                0x80 | // upper bit is always 1
                _state.lcdMode | // cached LCD mode value (lower 2 bits)
                (_state.lyCoincident ? 0x04 : 0);
        case 0xFF42: // SCY - BG Scroll Y
            return _state.scrollY;
        case 0xFF43: // SCX - BG Scroll X
            return _state.scrollX;
        case 0xFF44: // LY - LCD Current Scanline
            return _state.ly;
        case 0xFF45: // LYC - LY Compare
            return _state.lyCompare;
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

    if (_gameBoy->IsCgb())
    {
        switch (addr)
        {
            case 0xFF4F: // VRAM Bank Select
                return _state.vramBank;
            case 0xFF68: // Background Palette Address
                return _state.cgbBgPalAddr | (_state.cgbIncBgPalAddr ? 0x80 : 0);
            case 0xFF69: // Background Palette Data
                if (_state.cgbBgPalAddr & 0x01)
                {
                    return
                        ((_state.cgbBgPal[_state.cgbBgPalAddr >> 1].g & 0x18) >> 3) |
                        (_state.cgbBgPal[_state.cgbBgPalAddr >> 1].b << 2);
                }
                else
                {
                    return
                        ((_state.cgbBgPal[_state.cgbBgPalAddr >> 1].g & 0x07) << 5) |
                        _state.cgbBgPal[_state.cgbBgPalAddr >> 1].r;
                }
            case 0xFF6A: // Sprite Palette Address
                return _state.cgbObjPalAddr | (_state.cgbIncObjPalAddr ? 0x80 : 0);
            case 0xFF6B: // Sprite Palette Data
                if (_state.cgbObjPalAddr & 0x01)
                {
                    return
                        ((_state.cgbObjPal[_state.cgbObjPalAddr >> 1].g & 0x18) >> 3) |
                        (_state.cgbObjPal[_state.cgbObjPalAddr >> 1].b << 2);
                }
                else
                {
                    return
                        ((_state.cgbObjPal[_state.cgbObjPalAddr >> 1].g & 0x07) << 5) |
                        _state.cgbObjPal[_state.cgbObjPalAddr >> 1].r;
                }
        }
    }

#ifdef TRACE
    std::cout << "Read from unmapped PPU register, addr=" << std::hex << int(addr) << std::endl;
#endif
    return 0xFF;
}

void GameBoyPpu::Reset()
{
    _state = {};
    _state.lcdPower = (!_gameBoy->IsBiosEnabled());
    _state.lcdMode = LcdModeFlag::HBlank;

    // CGB palette is all white
    for (int i = 0; i < 32; i++)
    {
        _state.cgbBgPal[i].r = _state.cgbObjPal[i].r = 0x1F;
        _state.cgbBgPal[i].g = _state.cgbObjPal[i].g = 0x1F;
        _state.cgbBgPal[i].b = _state.cgbObjPal[i].b = 0x1F;
    }

    _state.vramBank = 0;
}

void GameBoyPpu::StartRender()
{
    _fifoOam.Clear();
    _fifoBg.Clear();
    _fifoBg.length = 8;

    _fetcherOam.tick = 0;
    _fetcherBg.tick = 0;

    _fetchNextSprite = true;

    // latch window
    _windowEnable = ((_state.lcdControl & 0x20) != 0);
    _windowStartX = _state.windowX;
    _windowStartY = _state.windowY;
    _insideWindow = false;

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
        StartRender();
        _state.lyCoincident = (_state.ly == _state.lyCompare);
        CheckLcdStatusIrq();
        _state.sleepCycles = 0; // wake up
    }
    else // powering off
    {
        _state.tick = 0;
        _state.scanline = 0;
        _state.ly = 0;
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
            if (!_gameBoy->IsCgb())
            {
                // DMG Bug: Writing to FF41 enable STAT for everything momentarily
                _state.lcdStatus = 0xF8 | (_state.lcdStatus & 0x07); // only on DMG
                CheckLcdStatusIrq();
            }
            _state.lcdStatus = val & 0xF8; // lower 3 bits are read-only
            CheckLcdStatusIrq();
            return;
        case 0xFF42: // SCY - BG Scroll Y
            _state.scrollY = val;
            return;
        case 0xFF43: // SCX - BG Scroll X
            _state.scrollX = val;
            return;
        case 0xFF45: // LY (LCDC Y-Coordinate)
            _state.lyCompare = val;
            if (_state.lcdPower)
            {
                _state.lyCoincident = (_state.ly == _state.lyCompare);
                CheckLcdStatusIrq();
                _state.sleepCycles = 0;
            }
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

    if (_gameBoy->IsCgb())
    {
        switch (addr)
        {
            case 0xFF4F: // VRAM Bank Select
                _state.vramBank = val & 0x01;
                return;
            case 0xFF68: // Background Palette Address
                _state.cgbBgPalAddr = val & 0x3F;
                _state.cgbIncBgPalAddr = (val & 0x80) != 0;
                return;
            case 0xFF69: // Background Palette Data
                if (_state.lcdMode != LcdModeFlag::Drawing)
                {
                    // XBBBBBGG GGGRRRRR
                    if (_state.cgbBgPalAddr & 0x01) // access high byte
                    {
                        _state.cgbBgPal[_state.cgbBgPalAddr >> 1].g =
                            (_state.cgbBgPal[_state.cgbBgPalAddr >> 1].g & 0x07) |
                            ((val & 0x03) << 3);
                        _state.cgbBgPal[_state.cgbBgPalAddr >> 1].b = (val >> 2) & 0x1F;
                    }
                    else
                    {
                        _state.cgbBgPal[_state.cgbBgPalAddr >> 1].r = val & 0x1F;
                        _state.cgbBgPal[_state.cgbBgPalAddr >> 1].g =
                            (_state.cgbBgPal[_state.cgbBgPalAddr >> 1].g & 0x18) |
                            (val >> 5);
                    }
                    _state.cgbBgPalAddr += _state.cgbIncBgPalAddr ? 1 : 0;
                    _state.cgbBgPalAddr &= 0x3F; // wrap
                }
                return;
            case 0xFF6A: // Sprite Palette Address
                _state.cgbObjPalAddr = val & 0x3F;
                _state.cgbIncObjPalAddr = (val & 0x80) != 0;
                return;
            case 0xFF6B: // Sprite Palette Data
                if (_state.lcdMode != LcdModeFlag::Drawing)
                {
                    // XBBBBBGG GGGRRRRR
                    if (_state.cgbObjPalAddr & 0x01)
                    {
                        _state.cgbObjPal[_state.cgbObjPalAddr >> 1].g =
                            (_state.cgbObjPal[_state.cgbObjPalAddr >> 1].g & 0x07) |
                            ((val & 0x03) << 3);
                        _state.cgbObjPal[_state.cgbObjPalAddr >> 1].b = (val >> 2) & 0x1F;
                    }
                    else
                    {
                        _state.cgbObjPal[_state.cgbObjPalAddr >> 1].r = val & 0x1F;
                        _state.cgbObjPal[_state.cgbObjPalAddr >> 1].g =
                            (_state.cgbObjPal[_state.cgbObjPalAddr >> 1].g & 0x18) |
                            (val >> 5);
                    }
                    _state.cgbObjPalAddr += (_state.cgbIncObjPalAddr) ? 1 : 0;
                    _state.cgbObjPalAddr &= 0x3F; // wrap
                }
                return;
        }
    }

#ifdef TRACE
    std::cout << "Write to unmapped PPU register, addr=" << std::hex << int(addr) << std::endl;
#endif
}

u8 GameBoyPpu::ReadVideoRam(u16 addr)
{
    if (_state.lcdMode == LcdModeFlag::Drawing)
    {
        // VRAM is disallowed in mode 3
#ifdef TRACE
        std::cout << "Warning! Disallowed VRAM read at addr " << std::hex << int(addr) << std::endl;
#endif
        return 0xFF;
    }
    else
    {
        return _videoRam[(_state.vramBank << 13) | (addr & 0x1FFF)];
    }
}

void GameBoyPpu::WriteVideoRam(u16 addr, u8 val)
{
    if (_state.lcdMode == LcdModeFlag::Drawing)
    {
        // VRAM is disallowed in mode 3
#ifdef TRACE
        std::cout << "Warning! Disallowed VRAM write at addr " << std::hex << int(addr) << std::endl
#endif
    }
    else
    {
        _videoRam[(_state.vramBank << 13) | (addr & 0x1FFF)] = val;
    }
}

u8 GameBoyPpu::ReadOamRam(u8 addr)
{
    if ((addr < 160) && (_state.lcdMode <= LcdModeFlag::VBlank)) // if in DMA or V-Blank or H-Blank, reads are allowed
    {
        return _oamRam[addr];
    }
    else
    {
#ifdef TRACE
        std::cout << "Warning! Disallowed OAM read at addr " << std::hex << int(addr) << std::endl;
#endif
        return 0xFF;
    }
}

void GameBoyPpu::WriteOamRam(u8 addr, u8 val, bool dmaBypass)
{
    if ((addr < 160) && (dmaBypass || (_state.lcdMode <= LcdModeFlag::VBlank))) // if in DMA or V-Blank or H-Blank, writes are allowed
    {
        _oamRam[addr] = val;
    }
    else
    {
#ifdef TRACE
        std::cout << "Warning! Disallowed OAM read at addr " << std::hex << int(addr) << std::endl;
#endif
    }
}

void GameBoyPpu::LoadState(std::ifstream &inState)
{
    inState.read((char *)&_state, sizeof(PpuState));

    inState.read((char *)&_fifoBg, sizeof(PixelFifo));
    inState.read((char *)&_fifoOam, sizeof(PixelFifo));
    inState.read((char *)&_fetcherBg, sizeof(PpuFetcher));
    inState.read((char *)&_fetcherOam, sizeof(PpuFetcher));

    inState.read((char *)&_windowEnable, sizeof(_windowEnable));
    inState.read((char *)&_insideWindow, sizeof(_insideWindow));
    inState.read((char *)&_windowStartX, sizeof(_windowStartX));
    inState.read((char *)&_windowStartY, sizeof(_windowStartY));
    inState.read((char *)&_windowOffset, sizeof(_windowOffset));

    inState.read((char *)&_fetchNextSprite, sizeof(_fetchNextSprite));
    inState.read((char *)&_fetchOamAddr, sizeof(_fetchOamAddr));
    inState.read((char *)_spriteX, sizeof(_spriteX));
    inState.read((char *)_spriteAddr, sizeof(_spriteAddr));
    inState.read((char *)&_spritesFound, sizeof(_spritesFound));

    inState.read((char *)&_renderPaused, sizeof(_renderPaused));
    inState.read((char *)&_pixelsRendered, sizeof(_pixelsRendered));
    inState.read((char *)&_bgColumn, sizeof(_bgColumn));
}

void GameBoyPpu::SaveState(std::ofstream &outState)
{
    outState.write((char *)&_state, sizeof(PpuState));

    outState.write((char *)&_fifoBg, sizeof(PixelFifo));
    outState.write((char *)&_fifoOam, sizeof(PixelFifo));
    outState.write((char *)&_fetcherBg, sizeof(PpuFetcher));
    outState.write((char *)&_fetcherOam, sizeof(PpuFetcher));

    outState.write((char *)&_windowEnable, sizeof(_windowEnable));
    outState.write((char *)&_insideWindow, sizeof(_insideWindow));
    outState.write((char *)&_windowStartX, sizeof(_windowStartX));
    outState.write((char *)&_windowStartY, sizeof(_windowStartY));
    outState.write((char *)&_windowOffset, sizeof(_windowOffset));

    outState.write((char *)&_fetchNextSprite, sizeof(_fetchNextSprite));
    outState.write((char *)&_fetchOamAddr, sizeof(_fetchOamAddr));
    outState.write((char *)_spriteX, sizeof(_spriteX));
    outState.write((char *)_spriteAddr, sizeof(_spriteAddr));
    outState.write((char *)&_spritesFound, sizeof(_spritesFound));

    outState.write((char *)&_renderPaused, sizeof(_renderPaused));
    outState.write((char *)&_pixelsRendered, sizeof(_pixelsRendered));
    outState.write((char *)&_bgColumn, sizeof(_bgColumn));
}