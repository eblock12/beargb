#pragma once

#include "shared.h"

// prevent cycles
class GameBoy;

struct CpuState
{
    bool halted;

    u16 pc; // program counter
    u16 sp; // stack pointer

    u8 a;
    u8 b;
    u8 c;
    u8 d;
    u8 e;
    u8 h;
    u8 l;

    u8 flags;
};

enum class CpuFlag
{
    Zero = 1 << 7,
    AddSub = 1 << 6,
    HalfCarry = 1 << 5,
    Carry = 1 << 4
};

class GameBoyCpu
{
    private:
        GameBoy *_gameBoy;

        CpuState _state;
    public:
        GameBoyCpu(GameBoy *gameBoy);
        ~GameBoyCpu();

        void Reset();
        void RunOneInstruction();

        inline u8 Read(u16 addr);
        inline u8 ReadImm();
        inline void Write(u16 addr, u8 val);
};