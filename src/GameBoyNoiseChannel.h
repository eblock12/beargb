#pragma once
#include <fstream>
#include "shared.h"

class GameBoyApu;

struct NoiseChannelState
{
    bool enabled;

    s8 output;

    // internal counter for frequency
    u16 timer;

    // internal volume level (loaded from volume envelope)
    u8 volume;

    // internal volume envelope timer
    u8 envelopeTimer;

    // 15-bit linear feedback shift register (LFSR)
    u16 shiftRegister;

    // Sound length (6-bit)
    u8 length;

    // Length of envelope steps
    u8 envelopeLength;

    // Envelope up/down (attenuate/amplify)
    bool envelopeIncrease;

    // Default Envelope Value
    u8 envelopeVolume;

    // Selects Dividing Ratio of Frequency
    u8 dividingRatio;

    // 0: 15 steps, 1: 7 steps
    bool useShortStep;

    // Select the shift clock frequency
    u8 shiftFrequency;

    // When enabled, length in FF20 is respected, otherwise is ignored
    bool lengthEnable;
};

class GameBoyNoiseChannel
{
private:
    NoiseChannelState _state;
    GameBoyApu *_apu;

    void ResetTimer();
public:
    GameBoyNoiseChannel(GameBoyApu *apu);
    ~GameBoyNoiseChannel();

    inline s8 GetOutput() { return _state.output; }
    inline u8 GetTimer() { return _state.timer; }

    bool IsEnabled() { return _state.enabled; }

    void Execute(u32 cycles);
    void TickCounter();
    void TickVolumeEnvelope();

    u8 ReadRegister(u16 addr);
    void WriteRegister(u16 addr, u8 val);

    void LoadState(std::ifstream &inState);
    void SaveState(std::ofstream &outState);
};