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
    // TODO: Process interrupts

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

        switch (opcode)
        {
            case 0x00: // NOP
                break;
            case 0x01: // LD BC,d16
                _regBC.SetWord(ReadImmWord());
                break;
            case 0x05: // DEC B
                DEC(_state.b);
                break;
            case 0x06: // LD B,d8
                _state.b = ReadImm();
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
            case 0x20: // JR NZ,r8
                JR(!GetFlag(CpuFlag::Zero), ReadImm());
                break;
            case 0x21: // LD HL,d16
                _regHL.SetWord(ReadImmWord());
                break;
            case 0x2A: // LD A,(HL+)
                _state.a = Read(_regHL.GetWord()); _regHL.Increment();
                break;
            case 0x31: // LD SP,d16
                _state.sp = ReadImmWord();
                break;
            case 0x32: // LD (HL-),A
                Write(_regHL.GetWord(), _state.a); _regHL.Decrement();
                break;
            case 0x36: // LD (HL),d8
                Write(_regHL.GetWord(), ReadImm());
                break;
            case 0x3E: // LD A,d8
                _state.a = ReadImm();
                break;
            case 0x78: // LD A,B
                _state.a = _state.b;
                break;
            case 0xAF: // XOR A
                XOR(_state.a);
                break;
            case 0xB1: // OR C
                OR(_state.c);
                break;
            case 0xC3: // JP a16
                JP(ReadImmWord());
                break;
            case 0xC9: // RET
                RET();
                break;
            case 0xCD: // CALL a16
                CALL(ReadImmWord());
                break;
            case 0xE0: // LDH (a8),A
                Write(0xFF00 | ReadImm(), _state.a);
                break;
            case 0xE2: // LD (C),A
                Write(0xFF00 | _state.c, _state.a);
                break;
            case 0xEA: // LD (a16),A
                Write(ReadImmWord(), _state.a);
                break;
            case 0xF0: // LDH A,(a8)
                _state.a = Read(0xFF00 | ReadImm());
                break;
            case 0xFB: // EI
                _state.pendingIME = true;
                break;
            case 0xF3: // DI
                _state.ime = false;
                break;
            case 0xFE: // CP d8
                CP(ReadImm());
                break;
            default:
                std::cout << "HALT! Unhandled opcode: " << std::uppercase << std::hex << int(opcode) << std::endl;
                _state.halted = true;
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

void GameBoyCpu::AND(u8 val)
{
    _state.a &= val;
    SetFlag(CpuFlag::Zero, _state.a == 0);
    ClearFlag(CpuFlag::AddSub);
    ClearFlag(CpuFlag::HalfCarry);
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

void GameBoyCpu::INC(u8 &reg)
{
    SetFlag(CpuFlag::HalfCarry, ((reg ^ 1 ^ (reg + 1)) & 0x10) != 0);
    reg++;
    SetFlag(CpuFlag::Zero, reg == 0);
    ClearFlag(CpuFlag::AddSub);
}

void GameBoyCpu::JP(u16 addr)
{
    _state.pc = addr;
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

void GameBoyCpu::RET()
{
    _state.pc = PopWord();
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->ExecuteTwoCycles();
}

void GameBoyCpu::XOR(u8 val)
{
    _state.a ^= val;
    SetFlag(CpuFlag::Zero, _state.a == 0);
    ClearFlag(CpuFlag::AddSub);
    ClearFlag(CpuFlag::HalfCarry);
    ClearFlag(CpuFlag::Carry);
}