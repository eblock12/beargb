#pragma once

#include "shared.h"
#include "IHostSystem.h"
#include "GameBoyNoiseChannel.h"
#include "GameBoySquareChannel.h"
#include "GameBoyWaveChannel.h"
#include "Blip_Buffer.h"
#include <memory>
#include <iostream>
#include <fstream>


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

    // high bit of $FF26, other enable bits are stored in channel instances
    bool masterEnable;

    u8 timerTick;
};

class GameBoyApu
{
private:
    static constexpr u32 OutputBufferSampleSize = 4096;

    GameBoy *_gameBoy;
    IHostSystem *_host;

    std::unique_ptr<GameBoySquareChannel> _square0;
    std::unique_ptr<GameBoySquareChannel> _square1;
    std::unique_ptr<GameBoyNoiseChannel> _noise;
    std::unique_ptr<GameBoyWaveChannel> _wave;

    ApuState _state;

    s32 _pendingCycles;
    u32 _cycleCount;

    blip_sample_t *_sampleBuffer;

    // used for delta calculations
    u16 _lastLeftSample;
    u16 _lastRightSample;

    Blip_Buffer _bufLeft;
    Blip_Buffer _bufRight;

    // band-limited synths
    Blip_Synth<blip_good_quality,32767> _synthLeft;
    Blip_Synth<blip_good_quality,32767> _synthRight;
public:
    GameBoyApu(GameBoy *gameBoy, IHostSystem *host);
    ~GameBoyApu();

    void AddCycles(s32 cycles) { _pendingCycles += cycles; }

    bool IsEnabled() { return _state.masterEnable; }

    void Execute();
    void TimerTick();

    u8 ReadRegister(u16 addr);
    void WriteRegister(u16 addr, u8 val);

    void LoadState(std::ifstream &inState);
    void SaveState(std::ofstream &outState);
};