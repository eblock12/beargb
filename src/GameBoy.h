#pragma once

#include "shared.h"

enum class GameBoyModel
{
    GameBoy,
    GameBoyColor,
    SuperGameBoy,
};

class GameBoy
{
private:
    static constexpr u32 WorkRamSize = 0x2000; // 8 KB
    static constexpr u32 WorkRamSizeCgb = 0x8000; // 32 KB
    static constexpr u32 VideoRamSize = 0x4000; // 16 KB
    static constexpr u32 VideoRamSizeCgb = 0x2000; // 8 KB
	static constexpr u32 OamRamSize = 0xA0;
	static constexpr u32 HighRamSize = 0x7F;

    GameBoyModel _model;

    // internal RAM
    u8 *_workRam = nullptr;
    u8 *_videoRam = nullptr;
    u8 *_oamRam = nullptr;
    u8 *_highRam = nullptr;

    // cartridge
    u8 *_prgRom = nullptr;
    u8 *_cartRam = nullptr;

    // depends on model selected
    u32 _workRamSize = 0;
    u32 _videoRamSize = 0;
public:
    GameBoy(GameBoyModel type);
    ~GameBoy();
};