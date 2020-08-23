#include "shared.h"
#include "GameBoySquareChannel.h"
#include <memory>

class GameBoy;

struct ApuState
{
    // Bit 0-2: Right Channel Volume
    // Bit 3: Enable External Right Channel
    // Bit 4-6: Left Channel Volume
    // Bit 7: Enable External Left Channel
    u8 masterVolume;

    // Bit 0: Square 0 Enable (Right Output)
    // Bit 1: Square 1 Enable (Right Output)
    // Bit 2: Wave Enable (Right Output)
    // Bit 3: Noise Enable (Right Output)
    // Bit 4: Square 0 Enable (Left Output)
    // Bit 5: Square 1 Enable (Left Output)
    // Bit 6: Wave Enable (Left Output)
    // Bit 7: Noise Enable (Left Output)
    u8 outputEnable;

    // Bit 0: Square 0 on/off
    // Bit 1: Square 1 on/off
    // Bit 2: Wave on/off
    // Bit 3: Noise on/off
    // Bit 7: Master on/off
    u8 channelEnable;
};

class GameBoyApu
{
private:
    static constexpr u32 OutputBufferSampleSize = 4096;

    GameBoy *_gameBoy;

    std::unique_ptr<GameBoySquareChannel> _square0;
    std::unique_ptr<GameBoySquareChannel> _square1;

    ApuState _state;

    u32 _pendingCycles;
    u32 _cycleCount;

    s16 *_sampleBuffer;
    u32 _bufferPosition;

    void Execute();
public:
    GameBoyApu(GameBoy *gameBoy);
    ~GameBoyApu();

    void AddCycles(u32 cycles) { _pendingCycles += cycles; }

    u8 ReadRegister(u16 addr);
    void WriteRegister(u16 addr, u8 val);
};