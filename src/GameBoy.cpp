#include "GameBoy.h"
#include <algorithm>
#include <iostream>

GameBoy::GameBoy(GameBoyModel model, const char *romFile, IHostSystem *host)
{
    _model = model;
    _host = host;

    _cart.reset(GameBoyCart::CreateFromRomFile(romFile, this));

    // auto model detection
    if (_model == GameBoyModel::Auto)
    {
        ModelSupport cgbSupport = _cart->GetColorGameBoySupport();
        if ((cgbSupport == ModelSupport::Optional) ||
            (cgbSupport == ModelSupport::Required))
        {
            _model = GameBoyModel::GameBoyColor;
        }
        else
        {
            _model = GameBoyModel::GameBoy;
        }
    }

    _state.isCgb = (_model == GameBoyModel::GameBoyColor);
    _state.cgbHighSpeed = false;

    _workRamSize = _state.isCgb ? GameBoy::WorkRamSizeCgb : GameBoy::WorkRamSize;
    _videoRamSize = _state.isCgb ? GameBoy::VideoRamSizeCgb : GameBoy::VideoRamSize;

    _workRam = new u8[_workRamSize];
    _highRam = new u8[GameBoy::HighRamSize];
    _videoRam = new u8[_videoRamSize];
    _oamRam = new u8[GameBoy::OamRamSize];

    _cpu.reset(new GameBoyCpu(this));
    _ppu.reset(new GameBoyPpu(this, host, _videoRam, _oamRam));
    _apu.reset(new GameBoyApu(this, host));

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
    _state.cycleCount++;
    _apu->AddCycles(_state.cgbHighSpeed ? 1 : 2);
    ExecuteTimer();
    _ppu->ExecuteCycle(); // Adjust for CGB
    _state.cycleCount++;
    if (!_state.cgbHighSpeed)
    {
        _ppu->ExecuteCycle();
    }
    if ((_state.cycleCount & 0x3) == 0) // run DMA on 4 cycle intervals
    {
        ExecuteOamDma();
    }
}

void GameBoy::Reset()
{
    _cart->Reset();
    _cpu->Reset();
    _ppu->Reset();

    MapMemory(_workRam, 0xC000, 0xDFFF, false /*readOnly*/);
    MapMemory(_workRam, 0xE000, 0xFFFF, false /*readOnly*/);
    MapRegisters(0x8000, 0x9FFF, true /*canRead*/, true /*canWrite*/);
    MapRegisters(0xFE00, 0xFFFF, true /*canRead*/, true /*canWrite*/);

    _cart->RefreshMemoryMap();

    _state.timerDivider = 1024;

    // skip BIOS by initializing registers to known values
    // this is correct for DMG but not later models
    WriteRegister(0xFF05, 0x00); // TIMA
    WriteRegister(0xFF06, 0x00); // TMA
    WriteRegister(0xFF07, 0x00); // TAC
    WriteRegister(0xFF10, 0x80); // NR10
    WriteRegister(0xFF11, 0xBF); // NR11
    WriteRegister(0xFF12, 0xF3); // NR12
    WriteRegister(0xFF14, 0xBF); // NR14
    WriteRegister(0xFF16, 0x3F); // NR21
    WriteRegister(0xFF17, 0x00); // NR22
    WriteRegister(0xFF19, 0xBF); // NR24
    WriteRegister(0xFF1A, 0x7F); // NR30
    WriteRegister(0xFF1B, 0xFF); // NR31
    WriteRegister(0xFF1C, 0x9F); // NR32
    WriteRegister(0xFF1E, 0xBF); // NR33
    WriteRegister(0xFF20, 0xFF); // NR41
    WriteRegister(0xFF21, 0x00); // NR42
    WriteRegister(0xFF22, 0x00); // NR43
    WriteRegister(0xFF23, 0xBF); // NR30
    WriteRegister(0xFF24, 0x77); // NR50
    WriteRegister(0xFF25, 0xF3); // NR51
    WriteRegister(0xFF26, 0xF1); // NR52
    WriteRegister(0xFF40, 0x91); // LCDC
    WriteRegister(0xFF42, 0x00); // SCY
    WriteRegister(0xFF43, 0x00); // SCX
    WriteRegister(0xFF45, 0x00); // LYC
    WriteRegister(0xFF47, 0xFC); // BGP
    WriteRegister(0xFF48, 0xFF); // OBP0
    WriteRegister(0xFF49, 0xFF); // OBP1
    WriteRegister(0xFF4A, 0x00); // WY
    WriteRegister(0xFF4B, 0x00); // WX
    WriteRegister(0xFFFF, 0x00); // IE
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

    _apu->Execute();
}

void GameBoy::SwitchSpeed()
{
    _state.cgbHighSpeed = !_state.cgbHighSpeed;
    _state.cgbPrepareSpeedSwitch = false;
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
                return _apu->ReadRegister(addr);
            case 0xFF40: case 0xFF41: case 0xFF42: case 0xFF43:
            case 0xFF44: case 0xFF45: case 0xFF47: case 0xFF48:
            case 0xFF49: case 0xFF4A: case 0xFF4B:
            case 0xFF4F: // CGB Bank
            case 0xFF68: case 0xFF69: case 0xFF6A: case 0xFF6B: // CGB Palette
                return _ppu->ReadRegister(addr);
            case 0xFF46: // DMA - OAM DMA Transfer
                return _state.oamDmaSrcAddr;
            case 0xFF4D: // CGB Speed Switch
                return (_state.cgbPrepareSpeedSwitch ? 0x01 : 0) |
                    (_state.cgbHighSpeed ? 0x80 : 0);
            case 0xFF55: // CGB DMA/HDMA Length
                return _state.cgbDmaLength | ( _state.cgbDmaComplete ? 0x80 : 0);
            case 0xFF70: // CGB WRAM Bank Register
                return _state.cgbRamBank;
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
                _apu->WriteRegister(addr, val);
                return;
            case 0xFF40: case 0xFF41: case 0xFF42: case 0xFF43:
            case 0xFF45: case 0xFF47: case 0xFF48: case 0xFF49:
            case 0xFF4A: case 0xFF4B:
            case 0xFF4F: // CGB Bank
            case 0xFF68: case 0xFF69: case 0xFF6A: case 0xFF6B: // CGB Palette
                _ppu->WriteRegister(addr, val);
                return;
            case 0xFF46: // OAM DMA Transfer and Start Address
                _state.oamDmaSrcAddr = val;
                _state.pendingOamDmaStart = true; // start DMA on next cycle
                return;
            case 0xFF4D: // CGB Speed Switch
                if (_state.isCgb)
                {
                    _state.cgbPrepareSpeedSwitch = (val & 0x01) != 0;
                }
                return;
            case 0xFF51: // CGB DMA/HDMA Source (High Byte)
                _state.cgbDmaSrcAddr = (_state.cgbDmaSrcAddr & 0xFF) | (val << 8);
                return;
            case 0xFF52: // CGB DMA/HDMA Source (Low Byte)
                _state.cgbDmaSrcAddr = (_state.cgbDmaSrcAddr & 0xFF00) | (val & 0xF0); // lower 4 bits are ignored
                return;
            case 0xFF53: // CGB DMA/HDMA Destination (High Byte)
                _state.cgbDmaDestAddr = (_state.cgbDmaDestAddr & 0xFF) | (val << 8);
                return;
            case 0xFF54: // CGB DMA/HDMA Destination (Low Byte)
                _state.cgbDmaDestAddr = (_state.cgbDmaDestAddr & 0xFF00) | (val & 0xF0); // lower 4 bits are ignored
                return;
            case 0xFF55: // CGB DMA/HDMA Length/Mode/Start
                _state.cgbDmaLength = val & 0x7F;

                if ((val & 0x80) != 0) // HDMA mode
                {
                    //std::cout << "HDMA: $" << std::hex << _state.cgbDmaSrcAddr << "->" << (0x8000 | _state.cgbDmaDestAddr) << " Length=" << int(_state.cgbDmaLength) << std::endl;
                    _state.cgbHdmaMode = true;
                    _state.cgbDmaComplete = false;
                }
                else // Regular DMA
                {
                    if (_state.cgbHdmaMode)
                    {
                        // starting a regular DMA transfer while in HDMA mode
                        // will abort the transfer
                        _state.cgbHdmaMode = false;
                        _state.cgbDmaComplete = true;
                    }
                    else
                    {
                       // std::cout << "DMA: $" << std::hex << _state.cgbDmaSrcAddr << "->" << (0x8000 | _state.cgbDmaDestAddr) << " Length=" << _state.cgbDmaLength << std::endl;

                        // 4 cycles burned during DMA initialization
                        ExecuteTwoCycles();
                        ExecuteTwoCycles();

                        do
                        {
                            ExecuteCgbDma();
                        } while (_state.cgbDmaLength != 0x7F);
                    }
                }
                return;
            case 0xFF70: // CGB WRAM Bank Register
                if (_state.isCgb)
                {
                    _state.cgbRamBank = val & 0x07;
                    if (_state.cgbRamBank == 0)
                    {
                        _state.cgbRamBank = 1;
                    }
                    MapMemory(_workRam + (_state.cgbRamBank * 0x1000), 0xD000, 0xDFFF, false /*readOnly*/);
                    MapMemory(_workRam + (_state.cgbRamBank * 0x1000), 0xF000, 0xFFFF, false /*readOnly*/);
                }
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

void GameBoy::ExecuteCgbDma()
{
    for (u8 i = 0; i < 16; i++) // DMA operates on 16 byte blocks
    {
        // this is running on the CPU so burn more machine cycles if in high-speed mode
        ExecuteTwoCycles();
        if (_state.cgbHighSpeed)
        {
            ExecuteTwoCycles();
        }
        Write(0x8000 | ((_state.cgbDmaDestAddr + i) & 0x1FFF), 
            Read(_state.cgbDmaSrcAddr + i));
    }

    _state.cgbDmaDestAddr += 16;
    _state.cgbDmaSrcAddr += 16;
    _state.cgbDmaLength--;
    _state.cgbDmaLength &= 0x7F; // ensure roll over

    if ((_state.cgbDmaLength == 0x7F) && _state.cgbHdmaMode) // if rolled over then transfer is complete
    {
        _state.cgbHdmaMode = false;
        _state.cgbDmaComplete = true;
    }
}

void GameBoy::ExecuteCgbHdma()
{
    if (_state.cgbHdmaMode)
    {
        // 4 cycles burned during DMA startup
        ExecuteTwoCycles();
        ExecuteTwoCycles();

        ExecuteCgbDma();
    }
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

    u16 mask = _state.cgbHighSpeed ? 0x2000 : 0x1000;
	if (((newDivider & mask) == 0) &&
        ((_state.divider & mask) != 0)) {
		_apu->TimerTick();
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