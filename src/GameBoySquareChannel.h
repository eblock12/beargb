#pragma once

class GameBoyApu;

#include <fstream>
#include "shared.h"

struct SquareChannelState
{
    // internal enable flag
    bool enabled;

    // output sample
    s8 output;

    // internal volume level
    u8 volume;

    // internal counter for frequency
    u16 timer;

    // internal volume envelope timer
    u8 envelopeTimer;

    // internal sweep frequency (shadow register)
    u16 sweepFrequency;

    // internal counter for sweep timer
    u16 sweepTimer;

    // internal flag for enabling sweep envelope
    bool sweepEnable;

    // Sweep Shift Number
    u8 sweepShift;

    // Sweep Increase/Decrease true = Subtraction, false = Addition
    bool sweepDecrease;

    // Sweep Length
    u8 sweepLength;

    // Sound length x (1/256) sec
    u8 length;

    // Waveform duty cycle selection
    u8 dutyCycleSelect;

    // internal position within the duty cycle table
    u8 dutyCyclePosition;

    // Length of envelope steps
    u8 envelopeLength;

    // Envelope up/down (attenuate/amplify)
    bool envelopeIncrease;

    // Default Envelope Value
    u8 envelopeVolume;

    // Internal flag to halt the volume envelope
    bool envelopeHalted;

    // Frequency (11-bit), really the period of the timer;
    u16 frequency;

    // When enabled, length in FF11 is respected, otherwise is ignored
    bool lengthEnable;
};

class GameBoySquareChannel
{
private:
    static constexpr s8 DutyTable[4][8] = {
        { 0, 1, 1, 1, 1, 1, 1, 1 }, // 12.5%
        { 0, 0, 1, 1, 1, 1, 1, 1 }, // 25%
        { 0, 0, 0, 0, 1, 1, 1, 1 }, // 50%
        { 0, 0, 0, 0, 0, 0, 1, 1 }  // 75%
    };

    GameBoyApu *_apu;

    SquareChannelState _state;

    inline u16 CalculateSweepFrequency();
public:
    GameBoySquareChannel(GameBoyApu *apu);
    ~GameBoySquareChannel();

    inline s8 GetOutput() { return _state.output; }
    inline u16 GetTimer() { return _state.timer; }

    bool IsEnabled() { return _state.enabled; }
    void SetEnabled(bool val) { _state.enabled = val; }

    void Execute(u32 cycles);
    void TickCounter();
    void TickFrequencyEnvelope();
    void TickVolumeEnvelope();

    u8 ReadRegister(u16 addr);
    void WriteRegister(u16 addr, u8 val);

    void LoadState(std::ifstream &inState);
    void SaveState(std::ofstream &outState);
};