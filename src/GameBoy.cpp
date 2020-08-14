#include "GameBoy.h"
#include <iostream>

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
    _ppu.reset(new GameBoyPpu(this, _videoRam, _oamRam));

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
    // TODO: Run DMA

    _ppu->ExecuteCycle();
}

void GameBoy::Reset()
{
    _state = {};

    _cart->Reset();
    _cpu->Reset();

    MapMemory(_workRam, 0xC000, 0xDFFF, false /*readOnly*/);
    MapMemory(_workRam, 0xE000, 0xFDFF, false /*readOnly*/);
    MapRegisters(0x8000, 0x9FFF, true /*canRead*/, true /*canWrite*/);
    MapRegisters(0xFE00, 0xFFFF, true /*canRead*/, true /*canWrite*/);
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
    u8 block = addr >> 8;
    if (_readableRegMap[block])
    {
        return ReadRegister(addr);
    }
    if (_readMap[block])
    {
        return _readMap[block][addr & 0xFF];
    }

    return 0x00; // open bus
}

u8 GameBoy::ReadRegister(u16 addr)
{
    if (addr == 0xFFFF) // IE - Interrupt Enable
    {
        return _state.interruptEnable;
    }
    else if (addr >= 0xFF80)
    {
        return _highRam[addr & 0x7F];
    }
    else if (addr >= 0xFF00)
    {
        switch (addr)
        {
            case 0xFF01: // SB - Serial transfer data
                return _state.serialTransfer;
            case 0xFF02: // SC - Serial control
                return _state.serialControl | 0x7E; // bits 1-6 are always set
            case 0xFF0F: // IF - Interrupt Flags
                return _state.interruptFlags | 0xE0; // upper 3 bits are always set
            case 0xFF10: case 0xFF11: case 0xFF12: case 0xFF13:
            case 0xFF14: case 0xFF16: case 0xFF17: case 0xFF18:
            case 0xFF19: case 0xFF1A: case 0xFF1B: case 0xFF1C:
            case 0xFF1D: case 0xFF1E: case 0xFF20: case 0xFF21:
            case 0xFF22: case 0xFF23: case 0xFF24: case 0xFF25:
            case 0xFF26: case 0xFF30: case 0xFF31: case 0xFF32:
            case 0xFF33: case 0xFF34: case 0xFF35: case 0xFF36:
            case 0xFF37: case 0xFF38: case 0xFF39: case 0xFF3A:
            case 0xFF3B: case 0xFF3C: case 0xFF3D: case 0xFF3E:
            case 0xFF3F:
                std::cout << "Warning! Unsupported APU read: " << std::hex << int(addr) << std::endl;
                return 0xFF;
            case 0xFF40: case 0xFF41: case 0xFF42: case 0xFF43:
            case 0xFF44: case 0xFF45: case 0xFF47: case 0xFF48:
            case 0xFF49: case 0xFF4A: case 0xFF4B:
                return _ppu->ReadRegister(addr);
        }
    }
    else if (addr >= 0xFE00)
    {
        // OAM area
    }
    else if (addr >= 0x8000 && addr <= 0x9FFF)
    {
        return _ppu->ReadVideoRam(addr);
    }

    std::cout << "Read from unmapped register, addr=" << std::hex << int(addr) << std::endl;
    return 0xFF;
}

void GameBoy::Write(u16 addr, u8 val)
{
    u8 block = addr >> 8;
    if (_writeableRegMap[block])
    {
        WriteRegister(addr, val);
    }
    else if (_writeMap[block])
    {
        //std::cout << "Wrote to block=" << int(block) << " addr=" << int(addr & 0xFF) << " val=" << int(val) << std::endl;
        _writeMap[block][addr & 0xFF] = val;
    }
    else
    {
        std::cout << "WARNING! Wrote to open bus, addr=" << std::hex << int(addr) << std::endl;
    }
}

void GameBoy::WriteRegister(u16 addr, u8 val)
{
    if (addr == 0xFFFF) // IE - Interrupt Enable
    {
        _state.interruptEnable = val;
        return;
    }
    else if (addr >= 0xFF80)
    {
        _highRam[addr & 0x7F] = val;
        return;
    }
    else if (addr >= 0xFF00)
    {
        switch (addr)
        {
            case 0xFF01: // SB - Serial transfer data
                _state.serialTransfer = val;
                return;
            case 0xFF02: // SC - Serial control
                _state.serialControl = val & 0x81; // only bits 0 and 7 are settable
                // TODO: Pretend to start transfer, raise interrupts, etc?
                return;
            case 0xFF0F: // IF - Interrupt Flags
                _state.interruptFlags = val & 0x1F; // only lower 5 bits are settable
                return;
            case 0xFF10: case 0xFF11: case 0xFF12: case 0xFF13:
            case 0xFF14: case 0xFF16: case 0xFF17: case 0xFF18:
            case 0xFF19: case 0xFF1A: case 0xFF1B: case 0xFF1C:
            case 0xFF1D: case 0xFF1E: case 0xFF20: case 0xFF21:
            case 0xFF22: case 0xFF23: case 0xFF24: case 0xFF25:
            case 0xFF26: case 0xFF30: case 0xFF31: case 0xFF32:
            case 0xFF33: case 0xFF34: case 0xFF35: case 0xFF36:
            case 0xFF37: case 0xFF38: case 0xFF39: case 0xFF3A:
            case 0xFF3B: case 0xFF3C: case 0xFF3D: case 0xFF3E:
            case 0xFF3F:
                std::cout << "Warning! Unsupported APU write: " << std::hex << int(addr) << std::endl;
                return;
            case 0xFF40: case 0xFF41: case 0xFF42: case 0xFF43:
            case 0xFF45: case 0xFF47: case 0xFF48: case 0xFF49:
            case 0xFF4A: case 0xFF4B:
                _ppu->WriteRegister(addr, val);
                return;
        }
    }
    else if (addr >= 0xFE00)
    {
        // OAM area
    }
    else if (addr >= 0x8000 && addr <= 0x9FFF)
    {
        _ppu->WriteVideoRam(addr, val);
        return;
    }

    std::cout << "Wrote to unmapped register, addr=" << std::hex << int(addr) << std::endl;
}

void GameBoy::MapMemory(u8 *src, u16 start, u16 end, bool readOnly)
{
    // NOTE: This assumes start and end are aligned to 256 byte block lengths

    // walk through each block and map "src" into it
    for (u32 addr = start; addr < end; addr += 0x100, src += 0x100)
    {
        u8 block = addr >> 8;
        _readMap[block] = src;

        if (!readOnly)
        {
            _writeMap[block] = src;
        }
    }
}

void GameBoy::MapRegisters(u16 start, u16 end, bool canRead, bool canWrite)
{
    for (u32 addr = start; addr < end; addr += 0x100)
    {
        u8 block = addr >> 8;
        _readableRegMap[block] = canRead;
        _writeableRegMap[block] = canWrite;
    }
}