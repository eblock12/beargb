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

    }

    return 0xFF;
}

void GameBoySquareChannel::WriteRegister(u16 addr, u8 val)
{
}