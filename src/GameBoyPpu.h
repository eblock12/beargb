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

    // X coordinate of the background at the upper left pixel of the LCD
    u8 scrollX;

    // Y coordinate of the background at the upper left pixel of the LCD
    u8 scrollY;

    // current LCD scanline (update is delayed versus internal scanline counter)
    u8 lcdCurrentScanline;

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

class GameBoyPpu
{
private:
    GameBoy *_gameBoy;
    PpuState _state;

    u8 *_videoRam = nullptr;
    u8 *_oamRam = nullptr;

    void SetLcdPower(bool enable);
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
};