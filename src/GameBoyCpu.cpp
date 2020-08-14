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
        u8 opcode = ReadImm();

        std::cout << "$" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << int(_state.pc - 1) << ' ' << GameBoyCpu::OpcodeNames[opcode] <<
            "\t | A=" << std::setw(2) << std::setfill('0') << int(_state.a) <<
            " B=" << std::setw(2) << std::setfill('0') << int(_state.b) <<
            " C=" << std::setw(2) << std::setfill('0') << int(_state.c) <<
            " D=" << std::setw(2) << std::setfill('0') << int(_state.d) <<
            " E=" << std::setw(2) << std::setfill('0') << int(_state.e) <<
            " H=" << std::setw(2) << std::setfill('0') << int(_state.h) <<
            " L=" << std::setw(2) << std::setfill('0') << int(_state.l) <<
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
            case 0x05: // DEC B
                DEC(_state.b);
                break;
            case 0x06: // LD B,d8
                _state.b = ReadImm();
                break;
            case 0x0D: // DEC C
                DEC(_state.c);
                break;
            case 0x0E: // LD C, 8
                _state.c = ReadImm();
                break;
            case 0x20: // JR NZ,r8
                JR(!GetFlag(CpuFlag::Zero), ReadImm());
                break;
            case 0x21: // LD HL,d16
                _regHL.SetWord(ReadImmWord());
                break;
            case 0x32: // LD (HL-),A
                Write(_regHL.GetWord(), _state.a); _regHL.Decrement();
                break;
            case 0x3E: // LD A,d8
                _state.a = ReadImm();
                break;
            case 0xAF: // XOR A
                XOR(_state.a);
                break;
            case 0xC3: // JP a16
                JP(ReadImmWord());
                break;
            case 0xE0: // LDH (a8),A
                Write(0xFF00 | ReadImm(), _state.a);
                break;
            case 0xF3: // DI
                _state.ime = false;
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

void GameBoyCpu::DEC(u8 &reg)
{
    SetFlag(CpuFlag::HalfCarry, (reg & 0x0F) == 0);
    reg--;
    SetFlag(CpuFlag::Zero, reg == 0);
    SetFlag(CpuFlag::AddSub);
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

void GameBoyCpu::XOR(u8 val)
{
    _state.a ^= val;
    SetFlag(CpuFlag::Zero, _state.a == 0);
    ClearFlag(CpuFlag::AddSub);
    ClearFlag(CpuFlag::HalfCarry);
    ClearFlag(CpuFlag::Carry);
}