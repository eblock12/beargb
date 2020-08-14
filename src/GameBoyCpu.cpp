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
            " Flags=" << (_state.flags & CpuFlag::Zero ? 'Z' : 'z') <<
            (_state.flags & CpuFlag::AddSub ? 'N' : 'n') <<
            (_state.flags & CpuFlag::HalfCarry ? 'H' : 'h') <<
            (_state.flags & CpuFlag::Carry ? 'C' : 'c') <<
            std::endl;

        switch (opcode)
        {
            case 0x00: // NOP
                break;
            case 0xC3: // JP a16
                JP(ReadImmWord());
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

void GameBoyCpu::JP(u16 addr)
{
    _state.pc = addr;
    _gameBoy->ExecuteTwoCycles();
}