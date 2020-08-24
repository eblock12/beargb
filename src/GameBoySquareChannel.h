#pragma once

class GameBoyApu;

#include "shared.h"

struct SquareChannelState
{
    bool enabled;

    // Sweep Shift Number
    u8 sweepShift;

    // Sweep Increase/Decrease true = Subtraction, false = Addition
    bool sweepDecrease;

    // Sweep Time
    u8 sweepTime;

    // Sound length x (1/256) sec
    u8 length;

    // Waveform duty cycle
    u8 dutyCycles;
};

class GameBoySquareChannel
{
private:
	static constexpr u8 DutyTable[4][8] = {
		{ 0, 1, 1, 1, 1, 1, 1, 1 }, // 12.5%
		{ 0, 0, 1, 1, 1, 1, 1, 1 }, // 25%
		{ 0, 0, 0, 0, 1, 1, 1, 1 }, // 50%
		{ 0, 0, 0, 0, 0, 0, 1, 1 }  // 75%
	};

    GameBoyApu *_apu;

    SquareChannelState _state;
public:
    GameBoySquareChannel(GameBoyApu *apu);
    ~GameBoySquareChannel();

    void Execute(u32 cycles);

    u8 ReadRegister(u16 addr);
    void WriteRegister(u16 addr, u8 val);
};