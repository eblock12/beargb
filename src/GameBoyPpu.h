#pragma once

#include "shared.h"

class GameBoy;

struct PpuState
{
    // cycle within the current scanline (range 0-456)
    u16 tick;

    // which scanline we're currently on (including non-visible lines, i.e. v-blank)
    u8 scanline;

    // Bit 7: LCD Power
    // Bit 6: Window Tile Map (0=9800-9BFF, 1=9C00-9FFF)
    // Bit 5: Window Enable
    // Bit 4: BG & Window Tileset (0=8800-97FF, 1=8000-8FFF)
    // Bit 3: BG Tile Map (0=9800-9BFF, 1=9C00,9FFF)
    // Bit 2: Sprite Size (0=8x8, 1=8x16)
    // Bit 1: Sprites Enabled
    // Bit 0: BG Enabled
    u8 lcdControl;
    bool lcdPower; // cache it for convenience

    // Bit 6: LY=LYC Check Enable
    // Bit 5: Mode 2 OAM Check Enable
    // Bit 4: Mode 1 V-Blank Check Enable
    // Bit 3: Mode 0 H-Blank Check Enable
    // Bit 2: LY=LYC Comparison Signal
    // Bit 1-0: Screen Mode
    //  Mode 00: H-Blank
    //  Mode 01: V-Blank
    //  Mode 10: Search OAM
    //  Mode 11: Drawing to LCD
    u8 lcdStatus;

    // keep track if the STAT irq was just raised
    bool raisedStatIrq;

    // X coordinate of the background at the upper left pixel of the LCD
    u8 scrollX;

    // Y coordinate of the background at the upper left pixel of the LCD
    u8 scrollY;

    // Current Y coordinate (update is delayed versus internal scanline counter)
    u8 ly;

    // Compared with LY and sets coincident flag on match
    u8 lyCompare;

    // Flag that is set when LY and LYC are matching
    bool lyCoincident;

    // Background & Window Palette Data
    u8 bgPal;

    // Object Palette 0 Data
    u8 objPal0;

    // Object Palette 1 Data
    u8 objPal1;

    // X coordinate of the upper left corner of the window
    u8 windowX;

    // Y coordinate of the upper left corner of the window
    u8 windowY;
};

namespace LcdStatusFlags
{
    enum LcdStatusFlags : u8
    {
        CoincidentScanline = 0x40,
        OamSearch = 0x20,
        VBlank = 0x10,
        HBlank = 0x08
    };
}

namespace LcdModeFlag
{
    enum LcdModeFlag : u8
    {
        Drawing = 0x03,
        OamSearch = 0x02,
        VBlank = 0x01,
        HBlank = 0x00
    };
}

struct PixelFifoEntry
{
    u8 color;
    u8 attributes;
};

struct PixelFifo
{
    PixelFifoEntry data[8] = {};
    u8 length;
    u8 position;

    void Clear()
    {
        for (int i = 0; i < 8; i++)
        {
            data[i] = {};
        }
        position = 0;
        length = 0;
    }

    void Pop()
    {
        length--;
        data[position] = {};
        position++;
        position = position & 7;
    }
};

struct PpuFetcher
{
    u8 tick;
    u8 tileData0;
    u8 tileData1;
    u8 attributes;
    u16 tileSetAddr;
};

class GameBoyPpu
{
private:
    GameBoy *_gameBoy;
    PpuState _state;

    u8 *_videoRam = nullptr;
    u8 *_oamRam = nullptr;

    PixelFifo _fifoBg;
    PixelFifo _fifoOam;
    PpuFetcher _fetcherBg;
    PpuFetcher _fetcherOam;

    bool _windowEnable;
    bool _insideWindow;
    u8 _windowStartX;
    u8 _windowStartY;
    u8 _windowOffset;

    bool _fetchNextSprite;
    u8 _fetchOamAddr;
    u8 _spriteX[10];
    u8 _spriteAddr[10];
    u8 _spritesFound;

    bool _renderPaused;
    s16 _pixelsRendered;
    u8 _bgColumn;
    u32 *_pixelBuffer;
    u32 _gbPal[4] =
    {
        0xFFFFFF00,
        0x80808000,
        0x40404000,
        0x00000000,
    };

    inline void StartRender();
    void SetLcdPower(bool enable);
    inline void TickDrawing();
    inline void TickBgFetcher();
    inline void TickOamSearch();
    inline void TickOamFetcher();
    inline void MoveToNextSprite();
    inline void CheckLcdStatusIrq();
public:
    GameBoyPpu(GameBoy *gameBoy, u8 *videoRam, u8 *oamRam);
    ~GameBoyPpu();

    void ExecuteCycle();
    u8 ReadRegister(u16 addr);
    void Reset();
    void WriteRegister(u16 addr, u8 val);

    u8 ReadVideoRam(u16 addr);
    void WriteVideoRam(u16 addr, u8 val);

    u8 ReadOamRam(u8 addr);
    void WriteOamRam(u8 addr, u8 val, bool dmaBypass);

    u32 *GetPixelBuffer() { return _pixelBuffer; }
};