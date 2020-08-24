#include "GameBoySquareChannel.h"
#include "GameBoyApu.h"

GameBoySquareChannel::GameBoySquareChannel(GameBoyApu *apu)
{
    _apu = apu;
}

GameBoySquareChannel::~GameBoySquareChannel()
{
}

void GameBoySquareChannel::Execute(u32 cycles)
{

}

u8 GameBoySquareChannel::ReadRegister(u16 addr)
{
    switch (addr)
    {
        case 0: // Sweep Envelope (FF10) - Square0 only
            return
                (_state.sweepShift & 0x07) |
                (_state.sweepDecrease ? 0x08 : 0x00) |
                ((_state.sweepTime & 0x07) << 4);

        case 1: // Duty Pattern & Length (FF11/FF16)
            return (_state.dutyCycles & 0x02) << 7; // only upper bit is readable??

        case 2: // Volume Envelope (FF12/FF17)
            return  0;
    }

    return 0xFF;
}

void GameBoySquareChannel::WriteRegister(u16 addr, u8 val)
{
    switch (addr)
    {
        case 0:  // Sweep Envelope $FF10) - Square0 only
            _state.sweepShift = val & 0x07;
            _state.sweepDecrease = (val & 0x08) != 0;
            _state.sweepTime = (val >> 4) & 0x07;
            break;

        case 1: // Duty Pattern & Length (FF10/FF16)
            _state.length = 64 - (val & 0x3F); // length = (64 -t1) x (1/256sec)
            _state.dutyCycles = (val >> 6) & 0x03;
            break;

        case 2: // Volume Envelope (FF12/FF17)
            break;
    }
}