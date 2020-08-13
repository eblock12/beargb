#include "GameBoyCpu.h"
#include "GameBoy.h"

#include <iostream>

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

        switch (opcode)
        {
            case 0x00: // NOP
                break;
            default:
                std::cout << "HALT! Unhandled opcode: " << std::hex << int(opcode) << std::endl;
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

void GameBoyCpu::Write(u16 addr, u8 val)
{
    _gameBoy->ExecuteTwoCycles();
    _gameBoy->Write(addr, val);
    _gameBoy->ExecuteTwoCycles();
}