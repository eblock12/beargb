#include "GameBoyCpu.h"
#include "GameBoy.h"

#include <iostream>
#include <iomanip>

GameBoyCpu::GameBoyCpu(GameBoy *gameBoy)
{
    _gameBoy = gameBoy;
}

GameBoyCpu::~GameBoyCpu()
{
}

void GameBoyCpu::Reset()
{
    _state = {};

    // skip the bios and just set expected state
    _state.pc = 0x0100;
    _state.sp = 0xFFFE;
    _state.flags = 0xB0;

    // initial register values vary depending on BIOS in each model, some games check this
    if (_gameBoy->GetModel() == GameBoyModel::GameBoyColor)
    {
        _state.a = 0x11;
        _state.d = 0xFF;
        _state.e = 0x56;
        _state.l = 0x0D;
    }
    else
    {
        _state.a = 0x01;
        _state.c = 0x13;
        _state.e = 0xD8;
        _state.h = 0x01;
        _state.l = 0x4D;
    }
}

void GameBoyCpu::RunOneInstruction()
{
    u8 pendingInterrupt = _gameBoy->GetPendingInterrupt();
    if (pendingInterrupt)
    {
        _state.halted = false; // cpu exits HALT even if IME is disabled

        if (_state.ime)
        {
            // two wait states
            _gameBoy->ExecuteTwoCycles();
            _gameBoy->ExecuteTwoCycles();
            _gameBoy->ExecuteTwoCycles();
            _gameBoy->ExecuteTwoCycles();

            // push current PC to stack
            u16 preservePC = _state.pc;
            PushWord(_state.pc);

            // loading new PC consumes two more machine cycles
            _gameBoy->ExecuteTwoCycles();
            _gameBoy->ExecuteTwoCycles();

            // check interrupt lines again in case the Push altered the registers
            pendingInterrupt = _gameBoy->GetPendingInterrupt();

            if (pendingInterrupt)
            {
                switch (pendingInterrupt)
                {
                    case IrqFlag::VBlank:
                        _state.pc = 0x0040;
                        break;
                    case IrqFlag::LcdStat:
                        _state.pc = 0x0048;
                        break;
                    case IrqFlag::Timer:
                        _state.pc = 0x0050;
                        break;
                    case IrqFlag::Serial:
                        _state.pc = 0x0058;
                        break;
                    case IrqFlag::JoyPad:
                        _state.pc = 0x0060;
                        break;
                }

                _gameBoy->ResetInterruptFlags(pendingInterrupt);
            }
            else
            {
                // interrupt was reset after processing started, we are screwed
                _state.pc = 0x0000;
            }

            _state.ime = false;
        }
    }

    if (_state.halted)
    {
        // just burn cycles
        _gameBoy->ExecuteTwoCycles();
        _gameBoy->ExecuteTwoCycles();
    }
    else
    {
        if (_state.pendingIME)
        {
            _state.pendingIME = false;
            _state.ime = true;
        }

        u8 opcode = ReadImm();
#if TRACE
        std::cout << "$" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << int(_state.pc - 1) << ' ' << GameBoyCpu::OpcodeNames[opcode] <<
            "\t | A=" << std::setw(2) << std::setfill('0') << int(_state.a) <<
            " B=" << std::setw(2) << std::setfill('0') << int(_state.b) <<
            " C=" << std::setw(2) << std::setfill('0') << int(_state.c) <<
            " D=" << std::setw(2) << std::setfill('0') << int(_state.d) <<
            " E=" << std::setw(2) << std::setfill('0') << int(_state.e) <<
            " HL=" << std::setw(4) << std::setfill('0') << int(_regHL.GetWord()) <<
            " SP=" << std::setw(4) << std::setfill('0') << int(_state.sp) <<
            " Flags=" << (GetFlag(CpuFlag::Zero) ? 'Z' : 'z') <<
            (GetFlag(CpuFlag::AddSub) ? 'N' : 'n') <<
            (GetFlag(CpuFlag::HalfCarry) ? 'H' : 'h') <<
            (GetFlag(CpuFlag::Carry) ? 'C' : 'c') <<
            std::endl;
#endif

        switch (opcode)
        {
            case 0x00: // NOP
                break;
            case 0x01: // LD BC,d16
                _regBC.SetWord(ReadImmWord());
                break;
            case 0x02: // LD (BC),A
                Write(_regBC.GetWord(), _state.a);
                break;
            case 0x03: // INC BC
                INC(_regBC);
                break;
            case 0x04: // INC B
                INC(_state.b);
                break;
            case 0x05: // DEC B
                DEC(_state.b);
                break;
            case 0x06: // LD B,d8
                _state.b = ReadImm();
                break;
            case 0x09: // ADD HL,BC
                ADD(_regHL, _regBC.GetWord());
                break;
            case 0x0A: // LD A,(BC)
                _state.a = Read(_regBC.GetWord());
                break;
            case 0x0B: // DEC BC
                DEC(_regBC);
                break;
            case 0x0C: // INC C
                INC(_state.c);
                break;
            case 0x0D: // DEC C
                DEC(_state.c);
                break;
            case 0x0E: // LD C,d8
                _state.c = ReadImm();
                break;
            case 0x11: // LD DE,d16
                _regDE.SetWord(ReadImmWord());
                break;
            case 0x12: // LD (DE),A
                Write(_regDE.GetWord(), _state.a);
                break;
            case 0x13: // INC DE
                INC(_regDE);
                break;
            case 0x14: // INC D
                INC(_state.d);
                break;
            case 0x16: // LD D,d8
                _state.d = ReadImm();
                break;
            case 0x18: // JR r8
                JR(ReadImm());
                break;
            case 0x19: // ADD HL,DE
                ADD(_regHL, _regDE.GetWord());
                break;
            case 0x1A: // LD A,(DE)
                _state.a = Read(_regDE.GetWord());
                break;
            case 0x1B: // DEC DE
                DEC(_regDE);
                break;
            case 0x1C: // INC E
                INC(_state.e);
                break;
            case 0x20: // JR NZ,r8
                JR(!GetFlag(CpuFlag::Zero), ReadImm());
                break;
            case 0x21: // LD HL,d16
                _regHL.SetWord(ReadImmWord());
                break;
            case 0x22: // LD (HL+),A
                Write(_regHL.GetWord(), _state.a); _regHL.Increment();
                break;
            case 0x23: // INC HL
                INC(_regHL);
                break;
            case 0x24: // INC H
                INC(_state.h);
                break;
            case 0x28: // JR Z,r8
                JR((_state.flags & CpuFlag::Zero) != 0, ReadImm());
                break;
            case 0x29: // ADD HL,HL
                ADD(_regHL, _regHL.GetWord());
                break;
            case 0x2A: // LD A,(HL+)
                _state.a = Read(_regHL.GetWord()); _regHL.Increment();
                break;
            case 0x2B: // DEC HL
                DEC(_regHL);
                break;
            case 0x2C: // INC L
                INC(_state.l);
                break;
            case 0x2F: // CPL
                CPL();
                break;
            case 0x31: // LD SP,d16
                _state.sp = ReadImmWord();
                break;
            case 0x32: // LD (HL-),A
                Write(_regHL.GetWord(), _state.a); _regHL.Decrement();
                break;
            case 0x33: // INC SP
                INC_SP();
                break;
            case 0x34: // INC (HL)
                INC_Indirect(_regHL.GetWord());
                break;
            case 0x35: // DEC (HL)
                DEC_Indirect(_regHL.GetWord());
                break;
            case 0x36: // LD (HL),d8
                Write(_regHL.GetWord(), ReadImm());
                break;
            case 0x39: // ADD HL,SP
                ADD(_regHL, _state.sp);
                break;
            case 0x3B: // DEC SP
                DEC_SP();
                break;
            case 0x3C: // INC A
                INC(_state.a);
                break;
            case 0x3D: // DEC A
                DEC(_state.a);
                break;
            case 0x3E: // LD A,d8
                _state.a = ReadImm();
                break;
            case 0x40: // LD B,B
                _state.b = _state.b;
                break;
            case 0x41: // LD B,C
                _state.b = _state.c;
                break;
            case 0x42: // LD B,D
                _state.b = _state.d;
                break;
            case 0x43: // LD B,E
                _state.b = _state.e;
                break;
            case 0x44: // LD B,H
                _state.b = _state.h;
                break;
            case 0x45: // LD B,L
                _state.b = _state.l;
                break;
            case 0x46: // LD B,(HL)
                _state.b = Read(_regHL.GetWord());
                break;
            case 0x47: // LD B,A
                _state.b = _state.a;
                break;
            case 0x48: // LD C,B
                _state.c = _state.b;
                break;
            case 0x49: // LD C,C
                _state.c = _state.c;
                break;
            case 0x4A: // LD C,D
                _state.c = _state.d;
                break;
            case 0x4B: // LD C,E
                _state.c = _state.e;
                break;
            case 0x4C: // LD C,H
                _state.c = _state.h;
                break;
            case 0x4D: // LD C,L
                _state.c = _state.l;
                break;
            case 0x4E: // LD C,(HL)
                _state.c = Read(_regHL.GetWord());
                break;
            case 0x4F: // LD C,A
                _state.c = _state.a;
                break;
            case 0x50: // LD D,B
                _state.d = _state.b;
                break;
            case 0x51: // LD D,C
                _state.d = _state.c;
                break;
            case 0x52: // LD D,D
                _state.d = _state.d;
                break;
            case 0x53: // LD D,E
                _state.d = _state.e;
                break;
            case 0x54: // LD D,H
                _state.d = _state.h;
                break;
            case 0x55: // LD D,L
                _state.d = _state.l;
                break;
            case 0x56: // LD D,(HL)
                _state.d = Read(_regHL.GetWord());
                break;
            case 0x57: // LD D,A
                _state.d = _state.a;
                break;
            case 0x58: // LD E,B
                _state.e = _state.b;
                break;
            case 0x59: // LD E,C
                _state.e = _state.c;
                break;
            case 0x5A: // LD E,D
                _state.e = _state.d;
                break;
            case 0x5B: // LD E,E
                _state.e = _state.e;
                break;
            case 0x5C: // LD E,H
                _state.e = _state.h;
                break;
            case 0x5D: // LD E,L
                _state.e = _state.l;
                break;
            case 0x5E: // LD E,(HL)
                _state.e = Read(_regHL.GetWord());
                break;
            case 0x5F: // LD E,A
                _state.e = _state.a;
                break;
            case 0x60: // LD H,B
                _state.h = _state.b;
                break;
            case 0x61: // LD H,C
                _state.h = _state.c;
                break;
            case 0x62: // LD H,D
                _state.h = _state.d;
                break;
            case 0x63: // LD H,E
                _state.h = _state.e;
                break;
            case 0x64: // LD H,H
                _state.h = _state.h;
                break;
            case 0x65: // LD H,L
                _state.h = _state.l;
                break;
            case 0x66: // LD H,(HL)
                _state.h = Read(_regHL.GetWord());
                break;
            case 0x67: // LD H,A
                _state.h = _state.a;
                break;
            case 0x68: // LD L,B
                _state.l = _state.b;
                break;
            case 0x69: // LD L,C
                _state.l = _state.c;
                break;
            case 0x6A: // LD L,D
                _state.l = _state.d;
                break;
            case 0x6B: // LD L,E
                _state.l = _state.e;
                break;
            case 0x6C: // LD L,H
                _state.l = _state.h;
                break;
            case 0x6D: // LD L,L
                _state.l = _state.l;
                break;
            case 0x6E: // LD L,(HL)
                _state.l = Read(_regHL.GetWord());
                break;
            case 0x6F: // LD L,A
                _state.l = _state.a;
                break;
            case 0x77: // LD (HL),A
                Write(_regHL.GetWord(), _state.a);
                break;
            case 0x78: // LD A,B
                _state.a = _state.b;
                break;
            case 0x79: // LD A,C
                _state.a = _state.c;
                break;
            case 0x7A: // LD A,D
                _state.a = _state.d;
                break;
            case 0x7B: // LD A,E
                _state.a = _state.e;
                break;
            case 0x7C: // LD A,H
                _state.a = _state.h;
                break;
            case 0x7D: // LD A,L
                _state.a = _state.l;
                break;
            case 0x7E: // LD A,(HL)
                _state.a = Read(_regHL.GetWord());
                break;
            case 0x7F: // LD A,A
                _state.a = _state.a;
                break;
            case 0x80: // ADD A,B
                ADD(_state.b);
                break;
            case 0x81: // ADD A,C
                ADD(_state.c);
                break;
            case 0x82: // ADD A,D
                ADD(_state.d);
                break;
            case 0x83: // ADD A,E
                ADD(_state.e);
                break;
            case 0x84: // ADD A,H
                ADD(_state.h);
                break;
            case 0x85: // ADD A,L
                ADD(_state.l);
                break;
            case 0x86: // ADD A,(HL)
                ADD(Read(_regHL.GetWord()));
                break;
            case 0x87: // ADD A,A
                ADD(_state.a);
                break;
            case 0xA0: // AND B
                AND(_state.b);
                break;
            case 0xA1: // AND C
                AND(_state.c);
                break;
            case 0xA2: // AND D
                AND(_state.d);
                break;
            case 0xA3: // AND E
                AND(_state.e);
                break;
            case 0xA4: // AND H
                AND(_state.h);
                break;
            case 0xA5: // AND L
                AND(_state.l);
                break;
            case 0xA6: // AND (HL)
                AND(Read(_regHL.GetWord()));
                break;
            case 0xA7: // AND A
                AND(_state.a);
                break;
            case 0xA8: // XOR B
                XOR(_state.b);
                break;
            case 0xA9: // XOR C
                XOR(_state.c);
                break;
            case 0xAA: // XOR D
                XOR(_state.d);
                break;
            case 0xAB: // XOR E
                XOR(_state.e);
                break;
            case 0xAC: // XOR H
                XOR(_state.h);
                break;
            case 0xAD: // XOR L
                XOR(_state.l);
                break;
            case 0xAE: // XOR (HL)
                XOR(Read(_regHL.GetWord()));
                break;
            case 0xAF: // XOR A
                XOR(_state.a);
                break;
            case 0xB0: // OR B
                OR(_state.b);
                break;
            case 0xB1: // OR C
                OR(_state.c);
                break;
            case 0xB2: // OR D
                OR(_state.d);
                break;
            case 0xB3: // OR E
                OR(_state.e);
                break;
            case 0xB4: // OR H
                OR(_state.h);
                break;
            case 0xB5: // OR L
                OR(_state.l);
                break;
            case 0xB6: // OR (HL)
                OR(Read(_regHL.GetWord()));
                break;
            case 0xB7: // OR A
                OR(_state.a);
                break;
            case 0xC0: // RET NZ
                RET((_state.flags & CpuFlag::Zero) == 0);
                break;
            case 0xC1: // POP BC
                POP(_regBC);
                break;
            case 0xC2: // JP NZ,a16
                JP((_state.flags & CpuFlag::Zero) == 0, ReadImmWord());
                break;
            case 0xC3: // JP a16
                JP(ReadImmWord());
                break;
            case 0xC5: // PUSH BC
                PUSH(_regBC);
                break;
            case 0xC8: // RET Z
                RET((_state.flags & CpuFlag::Zero) != 0);
                break;
            case 0xC9: // RET
                RET();
                break;
            case 0xCA: // JP Z,a16
                JP((_state.flags & CpuFlag::Zero) != 0, ReadImmWord());
                break;
            case 0xCB: // PREFIX
                PREFIX();
                break;
            case 0xCD: // CALL a16
                CALL(ReadImmWord());
                break;
            case 0xD1: // POP DE
                POP(_regDE);
                break;
            case 0xD2: // JP NC,a16
                JP((_state.flags & CpuFlag::Carry) == 0, ReadImmWord());
                break;
            case 0xD5: // PUSH DE
                PUSH(_regDE);
                break;
            case 0xD9: // RETI
                RETI();
                break;
            case 0xDA: // JP C,a16
                JP((_state.flags & CpuFlag::Carry) != 0, ReadImmWord());
                break;
            case 0xE0: // LDH (a8),A
                Write(0xFF00 | ReadImm(), _state.a);
                break;
            case 0xE1: // POP HL
                POP(_regHL);
                break;
            case 0xE2: // LD (C),A
                Write(0xFF00 | _state.c, _state.a);
                break;
            case 0xE5: // PUSH HL
                PUSH(_regHL);
                break;
            case 0xE6: // AND d8
                AND(ReadImm());
                break;
            case 0xE9: // JP (HL)
                _state.pc = _regHL.GetWord();
                break;
            case 0xEA: // LD (a16),A
                Write(ReadImmWord(), _state.a);
                break;
            case 0xEF: // RST 28H
                RST(0x28);
                break;
            case 0xF0: // LDH A,(a8)
                _state.a = Read(0xFF00 | ReadImm());
                break;
            case 0xF1: // POP AF
                POP(_regAF);
                break;
            case 0xF5: // PUSH AF
                PUSH(_regAF);
                break;
            case 0xFB: // EI
                _state.pendingIME = true;
                break;
            case 0xF3: // DI
                _state.ime = false;
                break;
            case 0xFA: // LD A,(a16)
                _state.a = Read(ReadImmWord());
                break;
            case 0xFE: // CP d8
                CP(ReadImm());
                break;
            default:
                std::cout << "HALT! Unhandled opcode: " << std::uppercase << std::hex << int(opcode) << std::endl;
                _state.halted = true;
                _state.ime = _state.pendingIME = false;
                break;
        }
    }
}

u8 GameBoyCpu::Read(u16 addr)
{
    _gameBoy->ExecuteTwoCycles();
    u8 val = _gameBoy->Read(addr);
    _gameBoy->ExecuteTwoCycles();
    return val;
}

u8 GameBoyCpu::ReadImm()
{
    _gameBoy->ExecuteTwoCycles();
    u8 opcode = _gameBoy->Read(_state.pc);
    _gameBoy->ExecuteTwoCycles();
    _state.pc++;
    return opcode;
}

u16 GameBoyCpu::ReadImmWord()
{
    u8 a = ReadImm();
    u8 b = ReadImm();
    return (b << 8) | a;
}

void GameBoyCpu::Write(u16 addr, u8 val)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->Write(addr, val);
    _gameBoy->ExecuteTwoCycles();
}

bool GameBoyCpu::GetFlag(CpuFlag flag)
{
    return (_state.flags & flag) != 0;
}

void GameBoyCpu::SetFlag(CpuFlag flag)
{
    _state.flags |= flag;
}

void GameBoyCpu::SetFlag(CpuFlag flag, bool val)
{
    if (val)
    {
        _state.flags |= flag;
    }
    else
    {
        _state.flags &= ~flag;
    }
}

void GameBoyCpu::ClearFlag(CpuFlag flag)
{
    _state.flags &= ~flag;
}

u8 GameBoyCpu::PopByte()
{
    u8 val = Read(_state.sp);
    _state.sp++;
    return val;
}

u16 GameBoyCpu::PopWord()
{
    u8 lowByte = PopByte();
    u8 highByte = PopByte();
    return (highByte << 8) | lowByte;
}

void GameBoyCpu::PushByte(u8 val)
{
    _state.sp--;
    Write(_state.sp, val);
}

void GameBoyCpu::PushWord(u16 val)
{
    PushByte(val >> 8);
    PushByte((u8)val);
}

void GameBoyCpu::ADD(u8 val)
{
    int sum = _state.a + val;
    SetFlag(CpuFlag::HalfCarry, ((_state.a ^ val ^ sum) & 0x10) != 0);
    _state.a = (u8)sum;
    SetFlag(CpuFlag::Zero, _state.a == 0);
    SetFlag(CpuFlag::Carry, sum > 0xFF);
    ClearFlag(CpuFlag::AddSub);
}

void GameBoyCpu::ADD(Reg16 &reg, u16 val)
{
    int sum = reg.GetWord() + val;
    SetFlag(CpuFlag::HalfCarry, ((reg.GetWord() ^ val ^ sum) & 0x1000) != 0);
    reg.SetWord((u16)sum);
    ClearFlag(CpuFlag::AddSub);
    SetFlag(CpuFlag::Carry, sum > 0xFFFF);
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
}

void GameBoyCpu::AND(u8 val)
{
    _state.a &= val;
    SetFlag(CpuFlag::Zero, _state.a == 0);
    ClearFlag(CpuFlag::AddSub);
    SetFlag(CpuFlag::HalfCarry);
    ClearFlag(CpuFlag::Carry);
}

void GameBoyCpu::CALL(u16 addr)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    PushWord(_state.pc);
    _state.pc = addr;
}

void GameBoyCpu::CP(u8 val)
{
    signed cp = (signed)_state.a - val;

    SetFlag(CpuFlag::Zero, (u8)cp == 0);
    SetFlag(CpuFlag::AddSub);
    SetFlag(CpuFlag::HalfCarry, ((_state.a ^ val ^ cp) & 0x10) != 0);
    SetFlag(CpuFlag::Carry, cp < 0);
}

void GameBoyCpu::CPL()
{
    _state.a ^= 0xFF;
    SetFlag(CpuFlag::AddSub);
    SetFlag(CpuFlag::HalfCarry);
}

void GameBoyCpu::DEC(u8 &reg)
{
    SetFlag(CpuFlag::HalfCarry, (reg & 0x0F) == 0);
    reg--;
    SetFlag(CpuFlag::Zero, reg == 0);
    SetFlag(CpuFlag::AddSub);
}

void GameBoyCpu::DEC(Reg16 &reg)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    reg.SetWord(reg.GetWord() - 1);
    // No flags are set in 16-bit mode
}

void GameBoyCpu::DEC_Indirect(u16 addr)
{
    u8 val = Read(addr);
    DEC(val);
    Write(addr, val);
}

void GameBoyCpu::DEC_SP()
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    _state.sp--;
}

void GameBoyCpu::INC(u8 &reg)
{
    SetFlag(CpuFlag::HalfCarry, ((reg ^ 1 ^ (reg + 1)) & 0x10) != 0);
    reg++;
    SetFlag(CpuFlag::Zero, reg == 0);
    ClearFlag(CpuFlag::AddSub);
}

void GameBoyCpu::INC(Reg16 &reg)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    reg.SetWord(reg.GetWord() + 1);
    // No flags are set in 16-bit mode
}

void GameBoyCpu::INC_Indirect(u16 addr)
{
    u8 val = Read(addr);
    INC(val);
    Write(addr, val);
}

void GameBoyCpu::INC_SP()
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    _state.sp++;
}

void GameBoyCpu::JP(u16 addr)
{
    _state.pc = addr;
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
}

void GameBoyCpu::JP(bool condition, u16 addr)
{
    if (condition)
    {
        _state.pc = addr;
        _gameBoy->ExecuteTwoCycles();
        _gameBoy->ExecuteTwoCycles();
    }
}

void GameBoyCpu::JR(s8 offset)
{
    _state.pc += offset;
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
}

void GameBoyCpu::JR(bool condition, s8 offset)
{
    if (condition)
    {
        _state.pc += offset;
        _gameBoy->ExecuteTwoCycles();
        _gameBoy->ExecuteTwoCycles();
    }
}

void GameBoyCpu::OR(u8 val)
{
    _state.a |= val;
    SetFlag(CpuFlag::Zero, _state.a == 0);
    ClearFlag(CpuFlag::AddSub);
    ClearFlag(CpuFlag::HalfCarry);
    ClearFlag(CpuFlag::Carry);
}

void GameBoyCpu::PREFIX()
{
    u8 opcode = ReadImm();

    switch(opcode)
    {
        case 0x37: // SWAP A
            SWAP(_state.a);
            break;

        case 0x80: // RES 0, B
            _state.b &= ~(1 << 0);
            break;
        case 0x81: // RES 0, C
            _state.c &= ~(1 << 0);
            break;
        case 0x82: // RES 0, D
            _state.d &= ~(1 << 0);
            break;
        case 0x83: // RES 0, E
            _state.e &= ~(1 << 0);
            break;
        case 0x84: // RES 0, H
            _state.h &= ~(1 << 0);
            break;
        case 0x85: // RES 0, L
            _state.l &= ~(1 << 0);
            break;
        case 0x86: // RES 0, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 0));
            break;
        case 0x87: // RES 0, A
            _state.a &= ~(1 << 0);
            break;
        case 0x88: // RES 1, B
            _state.b &= ~(1 << 1);
            break;
        case 0x89: // RES 1, C
            _state.c &= ~(1 << 1);
            break;
        case 0x8A: // RES 1, D
            _state.d &= ~(1 << 1);
            break;
        case 0x8B: // RES 1, E
            _state.e &= ~(1 << 1);
            break;
        case 0x8C: // RES 1, H
            _state.h &= ~(1 << 1);
            break;
        case 0x8D: // RES 1, L
            _state.l &= ~(1 << 1);
            break;
        case 0x8E: // RES 1, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 1));
            break;
        case 0x8F: // RES 1, A
            _state.a &= ~(1 << 1);
            break;
        case 0x90: // RES 2, B
            _state.b &= ~(1 << 2);
            break;
        case 0x91: // RES 2, C
            _state.c &= ~(1 << 2);
            break;
        case 0x92: // RES 2, D
            _state.d &= ~(1 << 2);
            break;
        case 0x93: // RES 2, E
            _state.e &= ~(1 << 2);
            break;
        case 0x94: // RES 2, H
            _state.h &= ~(1 << 2);
            break;
        case 0x95: // RES 2, L
            _state.l &= ~(1 << 2);
            break;
        case 0x96: // RES 2, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 2));
            break;
        case 0x97: // RES 2, A
            _state.a &= ~(1 << 2);
            break;
        case 0x98: // RES 3, B
            _state.b &= ~(1 << 3);
            break;
        case 0x99: // RES 3, C
            _state.c &= ~(1 << 3);
            break;
        case 0x9A: // RES 3, D
            _state.d &= ~(1 << 3);
            break;
        case 0x9B: // RES 3, E
            _state.e &= ~(1 << 3);
            break;
        case 0x9C: // RES 3, H
            _state.h &= ~(1 << 3);
            break;
        case 0x9D: // RES 3, L
            _state.l &= ~(1 << 3);
            break;
        case 0x9E: // RES 3, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 3));
            break;
        case 0x9F: // RES 3, A
            _state.a &= ~(1 << 3);
            break;
        case 0xA0: // RES 4, B
            _state.b &= ~(1 << 4);
            break;
        case 0xA1: // RES 4, C
            _state.c &= ~(1 << 4);
            break;
        case 0xA2: // RES 4, D
            _state.d &= ~(1 << 4);
            break;
        case 0xA3: // RES 4, E
            _state.e &= ~(1 << 4);
            break;
        case 0xA4: // RES 4, H
            _state.h &= ~(1 << 4);
            break;
        case 0xA5: // RES 4, L
            _state.l &= ~(1 << 4);
            break;
        case 0xA6: // RES 4, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 4));
            break;
        case 0xA7: // RES 4, A
            _state.a &= ~(1 << 4);
            break;
        case 0xA8: // RES 5, B
            _state.b &= ~(1 << 5);
            break;
        case 0xA9: // RES 5, C
            _state.c &= ~(1 << 5);
            break;
        case 0xAA: // RES 5, D
            _state.d &= ~(1 << 5);
            break;
        case 0xAB: // RES 5, E
            _state.e &= ~(1 << 5);
            break;
        case 0xAC: // RES 5, H
            _state.h &= ~(1 << 5);
            break;
        case 0xAD: // RES 5, L
            _state.l &= ~(1 << 5);
            break;
        case 0xAE: // RES 5, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 5));
            break;
        case 0xAF: // RES 5, A
            _state.a &= ~(1 << 5);
            break;
        case 0xB0: // RES 6, B
            _state.b &= ~(1 << 6);
            break;
        case 0xB1: // RES 6, C
            _state.c &= ~(1 << 6);
            break;
        case 0xB2: // RES 6, D
            _state.d &= ~(1 << 6);
            break;
        case 0xB3: // RES 6, E
            _state.e &= ~(1 << 6);
            break;
        case 0xB4: // RES 6, H
            _state.h &= ~(1 << 6);
            break;
        case 0xB5: // RES 6, L
            _state.l &= ~(1 << 6);
            break;
        case 0xB6: // RES 6, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 6));
            break;
        case 0xB7: // RES 6, A
            _state.a &= ~(1 << 6);
            break;
        case 0xB8: // RES 7, B
            _state.b &= ~(1 << 7);
            break;
        case 0xB9: // RES 7, C
            _state.c &= ~(1 << 7);
            break;
        case 0xBA: // RES 7, D
            _state.d &= ~(1 << 7);
            break;
        case 0xBB: // RES 7, E
            _state.e &= ~(1 << 7);
            break;
        case 0xBC: // RES 7, H
            _state.h &= ~(1 << 7);
            break;
        case 0xBD: // RES 7, L
            _state.l &= ~(1 << 7);
            break;
        case 0xBE: // RES 7, (HL)
            Write(_regHL.GetWord(), Read(_regHL.GetWord()) & ~(1 << 7));
            break;
        case 0xBF: // RES 7, A
            _state.a &= ~(1 << 7);
            break;
        default:
            std::cout << "HALT! Unhandled PREFIX opcode: " << std::uppercase << std::hex << int(opcode) << std::endl;
            _state.halted = true;
            _state.ime = _state.pendingIME = false;
            break;
    }
}

void GameBoyCpu::POP(Reg16 &reg)
{
    reg.SetWord(PopWord());
}

void GameBoyCpu::PUSH(Reg16 &reg)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    PushWord(reg.GetWord());
}

void GameBoyCpu::RET()
{
    _state.pc = PopWord();
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
}

void GameBoyCpu::RET(bool condition)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    if (condition)
    {
        _state.pc = PopWord();
        _gameBoy->ExecuteTwoCycles();
        _gameBoy->ExecuteTwoCycles();
    }
}

void GameBoyCpu::RETI()
{
    _state.pc = PopWord();
    _state.ime = true;
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
}

void GameBoyCpu::RST(u8 val)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
    PushWord(_state.pc);
    _state.pc = val;
}

void GameBoyCpu::SWAP(u8 &reg)
{
    reg = ((reg & 0x0F) << 4) | (reg >> 4);
    SetFlag(CpuFlag::Zero, reg == 0);
    ClearFlag(CpuFlag::AddSub);
    ClearFlag(CpuFlag::HalfCarry);
    ClearFlag(CpuFlag::Carry);
}

void GameBoyCpu::XOR(u8 val)
{
    _state.a ^= val;
    SetFlag(CpuFlag::Zero, _state.a == 0);
    ClearFlag(CpuFlag::AddSub);
    ClearFlag(CpuFlag::HalfCarry);
    ClearFlag(CpuFlag::Carry);
}