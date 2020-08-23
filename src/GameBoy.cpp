#include "GameBoy.h"
#include <iostream>

GameBoy::GameBoy(GameBoyModel model, const char *romFile, IHostSystem *host)
{
    _model = model;
    _host = host;

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
    _apu.reset(new GameBoyApu(this));

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

    _apu->AddCycles(2);
    ExecuteTimer();
    _ppu->ExecuteCycle();
    if ((_state.cycleCount & 0x3) == 0) // run DMA on 4 cycle intervals
    {
        ExecuteOamDma();
    }
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

    _cart->RefreshMemoryMap();

    _state.timerDivider = 1024;
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
            case 0xFF00: // P1
                return GetJoyPadState();
            case 0xFF01: // SB - Serial transfer data
                return _state.serialTransfer;
            case 0xFF02: // SC - Serial control
                return _state.serialControl | 0x7E; // bits 1-6 are always set
            case 0xFF04: // DIV - Divider Register
                return _state.divider >> 8;
            case 0xFF05: // TIMA - Timer Counter
                return _state.timerCounter;
            case 0xFF06: // TMA - Timer Modulo
                return _state.timerModulo;
            case 0xFF07: // TAC - Timer Control
                return _state.timerControl | 0xF8; // upper 5 bits are always set
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
                //std::cout << "Warning! Unsupported APU read: " << std::hex << int(addr) << std::endl;
                return 0xFF;
            case 0xFF40: case 0xFF41: case 0xFF42: case 0xFF43:
            case 0xFF44: case 0xFF45: case 0xFF47: case 0xFF48:
            case 0xFF49: case 0xFF4A: case 0xFF4B:
                return _ppu->ReadRegister(addr);
            case 0xFF46: // DMA - OAM DMA Transfer
                return _state.oamDmaSrcAddr;

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
    else
    {
        return _cart->ReadRegister(addr);
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
            case 0xFF00: // P1
                _state.joyPadInputSelect = val;
                return;
            case 0xFF01: // SB - Serial transfer data
                _state.serialTransfer = val;
                return;
            case 0xFF02: // SC - Serial control
                _state.serialControl = val & 0x81; // only bits 0 and 7 are settable
                // TODO: Pretend to start transfer, raise interrupts, etc?
                return;
            case 0xFF04: // DIV - Divider Register
                _state.divider = 0;
                return;
            case 0xFF05: // TIMA - Timer Counter
                // Quirk: "During the strange cycle [A] you can prevent the IF flag from being set and prevent the
                // TIMA from being reloaded from TMA by writing a value to TIMA"
                if (_state.timerResetPending)
                {
                    // abort reseting the timer if it was about to happen
                    _state.timerResetPending = false;
                }

                // Quirk: "If you write to TIMA during the cycle that TMA is being loaded to it [B], the write will be
                // ignored and TMA value will be written to TIMA instead."
                if (!_state.timerResetting)
                {
                    // ignore write if about to reset the counter
                    _state.timerCounter = val;
                }
                return;
            case 0xFF06: // TMA - Timer Modulo
                _state.timerModulo = val;
                if (_state.timerResetting)
                {
                    // Quirk: "If TMA is written the same cycle it is loaded to TIMA, TIMA is also loaded with that value."
                    _state.timerCounter = _state.timerModulo;
                }
                return;
            case 0xFF07: // TAC - Timer Control
                _state.timerControl = val;
                switch (val & 0x03)
                {
                    case 0:
                        _state.timerDivider = 512; // 4.096 KHz
                        break;
                    case 1:
                        _state.timerDivider = 8;  // 262.144 KHz
                        break;
                    case 2:
                        _state.timerDivider = 32; // 65.536 KHz
                        break;
                    case 3:
                        _state.timerDivider = 128; // 16.384 KHz
                        break;
                }
                // Quirk: "When changing TAC register value, if the old selected bit by the multiplexer was 0, the new one is
                // 1, and the new enable bit of TAC is set to 1, it will increase TIMA.""
                if ((_state.timerControl & 0x4) != 0)
                {

                }
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
                //std::cout << "Warning! Unsupported APU write: " << std::hex << int(addr) << std::endl;
                return;
            case 0xFF40: case 0xFF41: case 0xFF42: case 0xFF43:
            case 0xFF45: case 0xFF47: case 0xFF48: case 0xFF49:
            case 0xFF4A: case 0xFF4B:
                _ppu->WriteRegister(addr, val);
                return;
            case 0xFF46: // DMA Transfer and Start Address
                _state.oamDmaSrcAddr = val;
                _state.pendingOamDmaStart = true; // start DMA on next cycle
                //std::cout << "DMA scheduled\n";
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
    else
    {
        _cart->WriteRegister(addr, val);
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

void GameBoy::UnmapMemory(u16 start, u16 end)
{
    // NOTE: This assumes start and end are aligned to 256 byte block lengths

    // walk through each block and map "src" into it
    for (u32 addr = start; addr < end; addr += 0x100)
    {
        u8 block = addr >> 8;
        _readMap[block] = nullptr;
        _writeMap[block] = nullptr;
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

void GameBoy::UnmapRegisters(u16 start, u16 end)
{
    for (u32 addr = start; addr < end; addr += 0x100)
    {
        u8 block = addr >> 8;
        _readableRegMap[block] = false;
        _writeableRegMap[block] = false;
    }
}

u8 GameBoy::GetJoyPadState()
{
    // TODO: Return actual button states
    u8 buttons = 0x0F;

    if ((_state.joyPadInputSelect & 0x10) == 0) {
        buttons &= ~(_host->IsButtonPressed(HostButton::Right) ? 0x01 : 0);
        buttons &= ~(_host->IsButtonPressed(HostButton::Left) ? 0x02 : 0);
        buttons &= ~(_host->IsButtonPressed(HostButton::Up) ? 0x04 : 0);
        buttons &= ~(_host->IsButtonPressed(HostButton::Down) ? 0x08 : 0);
    }
    if ((_state.joyPadInputSelect & 0x20) == 0) {
        buttons &= ~(_host->IsButtonPressed(HostButton::A) ? 0x01 : 0);
        buttons &= ~(_host->IsButtonPressed(HostButton::B) ? 0x02 : 0);
        buttons &= ~(_host->IsButtonPressed(HostButton::Select) ? 0x04 : 0);
        buttons &= ~(_host->IsButtonPressed(HostButton::Start) ? 0x08 : 0);
    }

    return buttons | (_state.joyPadInputSelect & 0x30) | 0xC0;
}

u8 GameBoy::GetPendingInterrupt()
{
    u8 interruptPending = _state.interruptEnable & _state.interruptFlags; // if enabled AND requested
    if (interruptPending != 0)
    {
        if ((interruptPending & IrqFlag::VBlank) != 0)
        {
            return IrqFlag::VBlank;
        }
        else if ((interruptPending & IrqFlag::LcdStat) != 0)
        {
            return IrqFlag::LcdStat;
        }
        else if ((interruptPending & IrqFlag::Timer) != 0)
        {
            return IrqFlag::Timer;
        }
        else if ((interruptPending & IrqFlag::Serial) != 0)
        {
            return IrqFlag::Serial;
        }
        else if ((interruptPending & IrqFlag::JoyPad) != 0)
        {
            return IrqFlag::JoyPad;
        }
    }

    return 0;
}

void GameBoy::ResetInterruptFlags(u8 val)
{
    _state.interruptFlags &= ~val;
}

void GameBoy::SetInterruptFlags(u8 val)
{
    _state.interruptFlags |= val;
}

void GameBoy::ResetTimerCounter()
{
    // timer has overflowed and is being reset
    _state.timerCounter = _state.timerModulo;
    _state.interruptFlags |= IrqFlag::Timer; // raise timer interrupt

    // needed to emulate timing quirks around the timer being reset
    _state.timerResetPending = false;
    _state.timerResetting = true;
}

void GameBoy::ExecuteTimer()
{
    if ((_state.divider & 0x03) == 2)
    {
        _state.timerResetting = false;
        if (_state.timerResetPending)
        {
            ResetTimerCounter();
        }
    }

    u16 newDivider = _state.divider + 2;

    if (((_state.timerControl & 0x4) != 0) &&
        !(newDivider & _state.timerDivider) &&
        (_state.divider & _state.timerDivider))
    {
        _state.timerCounter++;
        if (_state.timerCounter == 0)
        {
            _state.timerResetPending = true;
        }
    }

    _state.divider = newDivider;
}

void GameBoy::ExecuteOamDma()
{
    if (!_cpu->IsHalted())
    {
        if (_state.oamDmaCounter > 0)
        {
            // first DMA cycle does not write since nothing has been fetched yet
            if (_state.oamDmaCounter < 161)
            {
                _ppu->WriteOamRam(160 - _state.oamDmaCounter, _state.oamDmaBuffer, true /*dmaBypass*/);

                //std::cout << "DMA write $" << int(160 - _state.oamDmaCounter) << "=" << int(_state.oamDmaBuffer) << std::endl;
            }

            _state.oamDmaCounter--;
            _state.oamDmaBuffer = Read((_state.oamDmaSrcAddr << 8) + (160 - _state.oamDmaCounter));
            //std::cout << "DMA read $" << int((_state.oamDmaSrcAddr << 8) + (160 - _state.oamDmaCounter)) << "=" << int(_state.oamDmaBuffer) << std::endl;
        }

        if (_state.pendingOamDmaStart)
        {
            _state.pendingOamDmaStart = false;
            _state.oamDmaCounter = 161;
            //std::cout << "DMA start\n";
        }
    }
}