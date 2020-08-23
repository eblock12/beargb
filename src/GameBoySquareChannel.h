class GameBoyApu;

#include "shared.h"

struct SquareChannelState
{
    bool enabled;
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