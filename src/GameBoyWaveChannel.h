#pragma once
#include "shared.h"

class GameBoyApu;

struct WaveChannelState
{
    bool enabled;

    s8 output;

    // internal counter for frequency
    u16 timer;

    // internal volume level (loaded from volume envelope)
    u8 volume;

    // Sound length (0 to 256)
    u16 length;

    // When enabled, length in FF1B is respected, otherwise is ignored
    bool lengthEnable;

    // DAC is powered or not
    bool dacPowered;

    // Frequency (11-bit)
    u16 frequency;

    // position in the wave RAM
    u8 position;

    // sample read from the wave RAM
    u8 waveBuffer;

    // wave sample storage
    u8 waveRam[0x10];
};

class GameBoyWaveChannel
{
private:
    WaveChannelState _state;
    GameBoyApu *_apu;
public:
    GameBoyWaveChannel(GameBoyApu *apu);
    ~GameBoyWaveChannel();

    inline s8 GetOutput() { return _state.output; }
    inline u8 GetTimer() { return _state.timer; }

    void Execute(u32 cycles);
    void TickCounter();
    void TickVolumeEnvelope();

    u8 ReadRegister(u16 addr);
    void WriteRegister(u16 addr, u8 val);

    u8 ReadWaveRam(u8 addr);
    void WriteWaveRam(u8 addr, u8 val);
};