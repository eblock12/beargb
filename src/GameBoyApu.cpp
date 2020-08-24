#include "GameBoyApu.h"
#include "GameBoy.h"

#include <iostream>
#include <memory.h>

GameBoyApu::GameBoyApu(GameBoy *gameBoy)
{
    _gameBoy = gameBoy;

    _square0.reset(new GameBoySquareChannel(this));
    _square1.reset(new GameBoySquareChannel(this));

    _cycleCount = 0;
    _pendingCycles = 0;

    _sampleBuffer = new s16[OutputBufferSampleSize * 2];
    memset(_sampleBuffer, 0, OutputBufferSampleSize * 2 * sizeof(s16));
    _bufferPosition = 0;
}

GameBoyApu::~GameBoyApu()
{
    delete[] _sampleBuffer;
}

void GameBoyApu::Execute()
{
    while (_pendingCycles > 0)
    {
        u32 step = 1;

        _pendingCycles -= step;
        _cycleCount += step;

        _square0->Execute(step);
        _square1->Execute(step);

        s16 leftSample = 0, rightSample = 0;

        _sampleBuffer[_bufferPosition++] = leftSample;
        _sampleBuffer[_bufferPosition++] = rightSample;

        if (_bufferPosition >= 0)
        {
            std::cout << "WARNING: Overflow in internal sample buffer!" << std::endl;
            _bufferPosition = 0;
        }
    }
}

u8 GameBoyApu::ReadRegister(u16 addr)
{
    switch (addr)
    {
        case 0xFF10: // Square 0 Sweep Envelope
        case 0xFF11: // Square 0 Duty Pattern & Length
        case 0xFF12: // Square 0 Volume Envelope
        case 0xFF13: // Square 0 Frequency (Low Byte)
        case 0xFF14: // Square 0 Frequency (High 3 bits + Initial/Counter)
            return _square0->ReadRegister(addr - 0xFF10);

        case 0xFF16: // Square 1 Duty Pattern & Length
        case 0xFF17: // Square 1 Volume Envelope
        case 0xFF18: // Square 1 Frequency (Low Byte)
        case 0xFF19: // Square 1 Frequency (High 3 bits + Initial/Counter)
            return _square1->ReadRegister(addr - 0xFF15);

        case 0xFF1A: // Wave enable
        case 0xFF1B: // Wave sound length
        case 0xFF1C: // Wave output level
        case 0xFF1D: // Frequency (Low Byte)
        case 0xFF1E: // Frequency (High 3 bits + Initial/Counter)
            return 0x00;

        case 0xFF20: // Noise length
        case 0xFF21: // Noise volume envelope
        case 0xFF22: // Noise polynomial counter
        case 0xFF23: // Noise Initial/Counter
            return 0x00;

        case 0xFF24: // Master volume + external audio enable
            return _state.masterVolume;

        case 0xFF25: // Output enable
            return _state.outputEnable;

        case 0xFF26: // Channel enable
            return _state.channelEnable |
                0x70; // bits 4-6 are open bus

        // Wave Sample Data (32 4-bit samples)
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
		case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            return 0x00;
    }

    return 0xFF;
}

void GameBoyApu::WriteRegister(u16 addr, u8 val)
{
    switch (addr)
    {
        case 0xFF10: // Square 0 Sweep Envelope
        case 0xFF11: // Square 0 Duty Pattern & Length
        case 0xFF12: // Square 0 Volume Envelope
        case 0xFF13: // Square 0 Frequency (Low Byte)
        case 0xFF14: // Square 0 Frequency (High 3 bits + Initial/Counter)
            _square0->WriteRegister(addr - 0xFF10, val);
            break;

        case 0xFF16: // Square 1 Duty Pattern & Length
        case 0xFF17: // Square 1 Volume Envelope
        case 0xFF18: // Square 1 Frequency (Low Byte)
        case 0xFF19: // Square 1 Frequency (High 3 bits + Initial/Counter)
            _square1->WriteRegister(addr - 0xFF15, val);
            break;

        case 0xFF1A: // Wave enable
        case 0xFF1B: // Wave sound length
        case 0xFF1C: // Wave output level
        case 0xFF1D: // Frequency (Low Byte)
        case 0xFF1E: // Frequency (High 3 bits + Initial/Counter)
            break;

        case 0xFF20: // Noise length
        case 0xFF21: // Noise volume envelope
        case 0xFF22: // Noise polynomial counter
        case 0xFF23: // Noise Initial/Counter
            break;

        case 0xFF24: // Master volume + external audio enable
            _state.masterVolume = val;
            break;

        case 0xFF25: // Output enable
            _state.outputEnable = val;
            break;

        case 0xFF26: // Channel enable
            _state.channelEnable = val;
            break;

        // Wave Sample Data (32 4-bit samples)
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            break;
    }
}