#pragma once

#include <memory>
#include <string>
#include "shared.h"
#include "GameBoyCart.h"
#include "GameBoyCpu.h"

enum class GameBoyModel
{
    GameBoy,
    GameBoyColor,
    SuperGameBoy,
};

struct GameBoyState
{
    u64 cycleCount;
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
    GameBoyState _state = {};

    std::unique_ptr<GameBoyCart> _cart;
    std::unique_ptr<GameBoyCpu> _cpu;

    // internal RAM
    u8 *_workRam = nullptr;
    u8 *_videoRam = nullptr;
    u8 *_oamRam = nullptr;
    u8 *_highRam = nullptr;

    // depends on model selected
    u32 _workRamSize = 0;
    u32 _videoRamSize = 0;

    // memory mapping (64 KB address space is divided into 256 blocks that are 256 bytes long)
    // each block maps to ROM or RAM both internally or cartridge, writes to I/O registers are intercepted earlier
    u8 *_readMap[0x100] = {};
    u8 *_writeMap[0x100] = {};
public:
    GameBoy(GameBoyModel type, const std::string &romFile);
    ~GameBoy();

    GameBoyModel GetModel() { return _model; }

    void ExecuteTwoCycles();
    void Reset();
    void RunCycles(u32 cycles);
    void RunOneFrame();

    u8 Read(u16 addr);
    void Write(u16 addr, u8 val);

    // maps "src" data into address ranges from start to end, if readOnly then only map into "read" mapping
    // NOTE: start/end must align to 256 byte blocks
    void MapMemory(u8 *src, u16 start, u16 end, bool readOnly);
};