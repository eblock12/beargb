#include "GameBoy.h"

GameBoy::GameBoy(GameBoyModel model, const std::string &romFile)
{
    _model = model;

    bool isCgb = (_model == GameBoyModel::GameBoyColor);
    _workRamSize = isCgb ? GameBoy::WorkRamSizeCgb : GameBoy::WorkRamSize;
    _videoRamSize = isCgb ? GameBoy::VideoRamSizeCgb : GameBoy::VideoRamSize;

    _workRam = new u8[_workRamSize];
    _highRam = new u8[GameBoy::HighRamSize];
    _videoRam = new u8[_videoRamSize];
    _oamRam = new u8[GameBoy::OamRamSize];

    _cart.reset(GameBoyCart::CreateFromRomFile(romFile, this));
    _cpu.reset(new GameBoyCpu(this));

    Reset();
}

GameBoy::~GameBoy()
{
    delete[] _workRam;
    delete[] _highRam;
    delete[] _videoRam;
    delete[] _oamRam;
}

void GameBoy::ExecuteTwoCycles()
{
    _state.cycleCount += 2;

    // TODO: Run Timers
    // TODO: Run PPU
    // TODO: Run DMA
}

void GameBoy::Reset()
{
    _state = {};

    _cart->Reset();
    _cpu->Reset();
}

void GameBoy::RunCycles(u32 cycles)
{
    u64 targetCycleCount = _state.cycleCount + cycles;

    while (targetCycleCount >= _state.cycleCount)
    {
        _cpu->RunOneInstruction();
    }
}

void GameBoy::RunOneFrame()
{
    // 154 scanlines per frame, 456 clocks per scanline
    RunCycles(154 * 456);
}

u8 GameBoy::Read(u16 addr)
{
    // TODO: Check if hitting I/O registers

    u8 block = addr >> 8;
    if (_readMap[block])
    {
        return _readMap[block][addr & 0xFF];
    }

    return 0x00; // open bus
}

void GameBoy::Write(u16 addr, u8 val)
{
}

void GameBoy::MapMemory(u8 *src, u16 start, u16 end, bool readOnly)
{
    // NOTE: This assumes start and end are aligned to 256 byte block lengths

    // walk through each block and map "src" into it
    for (u16 addr = start; addr < end; addr += 0x100, src += 0x100)
    {
        u8 block = addr >> 8;
        _readMap[block] = src;

        if (!readOnly)
        {
            _writeMap[block] = src;
        }
    }
}