#include "GameBoyApu.h"
#include "GameBoy.h"

#include <algorithm>
#include <iostream>
#include <memory.h>

//#define TRACE

GameBoyApu::GameBoyApu(GameBoy *gameBoy, IHostSystem *host)
{
    _gameBoy = gameBoy;
    _host = host;

    _square0.reset(new GameBoySquareChannel(this));
    _square1.reset(new GameBoySquareChannel(this));
    _noise.reset(new GameBoyNoiseChannel(this));
    _wave.reset(new GameBoyWaveChannel(this));

    _state = {};

    _cycleCount = 0;
    _pendingCycles = 0;

    _sampleBuffer = new blip_sample_t[OutputBufferSampleSize * 2];
    memset(_sampleBuffer, 0, sizeof(blip_sample_t) * OutputBufferSampleSize * 2);

    _lastLeftSample = 0;
    _lastRightSample = 0;

    u32 sampleRate = 44100;  // TODO: Get from host
    u32 clockRate = 4194304;

    // setup Blip Buffers
    _bufLeft.clear();
    _bufRight.clear();

    _bufLeft.sample_rate(sampleRate);
    _bufLeft.clock_rate(clockRate);
    _bufRight.sample_rate(sampleRate);
    _bufRight.clock_rate(clockRate);

    // setup Blip synths
    _synthLeft.output(&_bufLeft);
    _synthLeft.volume(0.5);
    _synthRight.output(&_bufRight);
    _synthRight.volume(0.5);
}

GameBoyApu::~GameBoyApu()
{
    delete[] _sampleBuffer;
}

s16 maxSample = 0;

void GameBoyApu::Execute()
{
    if (!_state.masterEnable)
    {
        // apu is disabled, just eat cycles
        _cycleCount += _pendingCycles;
        _pendingCycles = 0;
    }
    else
    {
        while (_pendingCycles > 0)
        {
            u32 step = std::min({
                _pendingCycles,
                (s32)_square0->GetTimer(),
                (s32)_square1->GetTimer(),
                (s32)_wave->GetTimer(),
                (s32)_noise->GetTimer()
            });

            if (step == 0)
            {
                step = 1;
            }

            _pendingCycles -= step;

            _square0->Execute(step);
            _square1->Execute(step);
            _wave->Execute(step);
            _noise->Execute(step);

            s16 leftSample = 0, rightSample = 0;

            leftSample += (_state.outputEnable & 0x10) ? _square0->GetOutput() : 0;
            leftSample += (_state.outputEnable & 0x20) ? _square1->GetOutput() : 0;
            leftSample += (_state.outputEnable & 0x40) ? _wave->GetOutput() : 0;
            leftSample += (_state.outputEnable & 0x80) ? _noise->GetOutput() : 0;
            leftSample *= (((_state.masterVolume >> 4) & 0x07) + 1) * 40;

            rightSample += (_state.outputEnable & 0x01) ? _square0->GetOutput() : 0;
            rightSample += (_state.outputEnable & 0x02) ? _square1->GetOutput() : 0;
            rightSample += (_state.outputEnable & 0x04) ? _wave->GetOutput() : 0;
            rightSample += (_state.outputEnable & 0x08) ? _noise->GetOutput() : 0;
            rightSample *= ((_state.masterVolume & 0x07) + 1) * 40;

            // send any delta samples to Blip synth
            if (_lastLeftSample != leftSample)
            {
                _synthLeft.offset_inline(_cycleCount, leftSample - _lastLeftSample);
                _lastLeftSample = leftSample;
            }
            if (_lastRightSample != rightSample)
            {
                _synthRight.offset_inline(_cycleCount, rightSample - _lastRightSample);
                _lastRightSample = rightSample;
            }

           _cycleCount += step;
        }
    }

    if (_cycleCount >= 20000)
    {
        _bufLeft.end_frame(_cycleCount);
        _bufRight.end_frame(_cycleCount);

        u32 samplesRead = _bufLeft.read_samples(_sampleBuffer, OutputBufferSampleSize, 1);
        _bufRight.read_samples(_sampleBuffer + 1, OutputBufferSampleSize, 1);

        _host->QueueAudio(_sampleBuffer, samplesRead);

        _cycleCount = 0;
    }
}

void GameBoyApu::TimerTick()
{
    // process events that are triggered by the system timer

    Execute();

    if (_state.masterEnable) // APU is enabled
    {
        if ((_state.timerTick & 0x01) == 0x00)
        {
            // every 2nd tick
            _square0->TickCounter();
            _square1->TickCounter();
            _wave->TickCounter();
            _noise->TickCounter();

            if ((_state.timerTick & 0x03) != 0x02) // every 4th tick
            {
                _square0->TickFrequencyEnvelope();
            }
        }
        else if (_state.timerTick == 7)
        {
            // every 8 ticks
            _square0->TickVolumeEnvelope();
            _square1->TickVolumeEnvelope();
            _noise->TickVolumeEnvelope();
        }

        _state.timerTick++;
        _state.timerTick &= 0x07;
    }
}

u8 GameBoyApu::ReadRegister(u16 addr)
{
#ifdef TRACE
    std::cout << "read_apu: " << std::hex << int(addr) << std::endl;
#endif

    Execute();

    switch (addr)
    {
        case 0xFF10: // Square 0 Sweep Envelope (NR10)
        case 0xFF11: // Square 0 Duty Pattern & Length (NR11)
        case 0xFF12: // Square 0 Volume Envelope (NR12)
        case 0xFF13: // Square 0 Frequency (Low Byte) (NR13)
        case 0xFF14: // Square 0 Frequency (High 3 bits + Initial/Counter) (NR14)
            return _square0->ReadRegister(addr - 0xFF10);

        case 0xFF16: // Square 1 Duty Pattern & Length (NR21)
        case 0xFF17: // Square 1 Volume Envelope (NR22)
        case 0xFF18: // Square 1 Frequency (Low Byte) (NR23)
        case 0xFF19: // Square 1 Frequency (High 3 bits + Initial/Counter) (NR24)
            return _square1->ReadRegister(addr - 0xFF15);

        case 0xFF1A: // Wave enable (NR30)
        case 0xFF1B: // Wave sound length (NR31)
        case 0xFF1C: // Wave output level (NR32)
        case 0xFF1D: // Frequency (Low Byte) (NR33)
        case 0xFF1E: // Frequency (High 3 bits + Initial/Counter) (NR34)
            return _wave->ReadRegister(addr - 0xFF1A);

        case 0xFF20: // Noise length (NR41)
        case 0xFF21: // Noise volume envelope (NR42)
        case 0xFF22: // Noise polynomial counter (NR43)
        case 0xFF23: // Noise Initial/Counter (NR44)
            return _noise->ReadRegister(addr - 0xFF20);

        case 0xFF24: // Master volume + external audio enable (NR50)
            return _state.masterVolume;

        case 0xFF25: // Output enable (NR51)
            return _state.outputEnable;

        case 0xFF26: // Channel enable (NR52)
            if (_state.masterEnable)
            {
                return 0x70 | // open bus
                    0x80 | // apu enabled
                    (_square0->IsEnabled() ? 0x01 : 0) |
                    (_square1->IsEnabled() ? 0x02 : 0) |
                    (_wave->IsEnabled() ? 0x04 : 0) |
                    (_noise->IsEnabled() ? 0x08 : 0);
            }
            else
            {
                return 0x70; // bits 4-6 are open bus
            }
        // Wave Sample Data (32 4-bit samples)
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
		case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            return _wave->ReadWaveRam(addr - 0xFF30);
    }

    return 0xFF;
}

void GameBoyApu::WriteRegister(u16 addr, u8 val)
{
    Execute();

    if (!_state.masterEnable)
    {
        switch (addr)
        {
            case 0xFF10: case 0xFF12: case 0xFF13: case 0xFF14:
            case 0xFF17: case 0xFF18: case 0xFF19:
            case 0xFF1A: case 0xFF1C: case 0xFF1D: case 0xFF1E:
            case 0xFF20: case 0xFF21: case 0xFF22: case 0xFF23:
            case 0xFF24:
            case 0xFF25:
                // writing is disabled here when master channel is off
                return;
        }
    }

#ifdef TRACE
    std::cout << "write_apu: " << std::hex << int(addr) << "=" << int(val) << std::endl;
#endif

    switch (addr)
    {
        case 0xFF10: // Square 0 Sweep Envelope (NR10)
        case 0xFF11: // Square 0 Duty Pattern & Length (NR11)
        case 0xFF12: // Square 0 Volume Envelope (NR12)
        case 0xFF13: // Square 0 Frequency (Low Byte) (NR13)
        case 0xFF14: // Square 0 Frequency (High 3 bits + Initial/Counter) (NR14)
            _square0->WriteRegister(addr - 0xFF10, val);
            break;

        case 0xFF16: // Square 1 Duty Pattern & Length (NR21)
        case 0xFF17: // Square 1 Volume Envelope (NR22)
        case 0xFF18: // Square 1 Frequency (Low Byte) (NR23)
        case 0xFF19: // Square 1 Frequency (High 3 bits + Initial/Counter) (NR24)
            _square1->WriteRegister(addr - 0xFF15, val);
            break;

        case 0xFF1A: // Wave enable (NR30)
        case 0xFF1B: // Wave sound length (NR31)
        case 0xFF1C: // Wave output level (NR32)
        case 0xFF1D: // Frequency (Low Byte) (NR33)
        case 0xFF1E: // Frequency (High 3 bits + Initial/Counter) (NR34)
            _wave->WriteRegister(addr - 0xFF1A, val);
            break;

        case 0xFF20: // Noise length (NR41)
        case 0xFF21: // Noise volume envelope (NR42)
        case 0xFF22: // Noise polynomial counter (NR43)
        case 0xFF23: // Noise Initial/Counter (NR44)
            _noise->WriteRegister(addr - 0xFF20, val);
            break;

        case 0xFF24: // Master volume + external audio enable (NR50)
            _state.masterVolume = val;
            break;

        case 0xFF25: // Output enable (NR51)
            _state.outputEnable = val;
            break;

        case 0xFF26: // Channel enable (NR52)
        {
            bool masterEnableNewVal = (val & 0x80) != 0;
            if (masterEnableNewVal != _state.masterEnable)
            {
                if (masterEnableNewVal)
                {
                    // turning on
                    _state.timerTick = 0;
                }
                else
                {
                    // turning off
                    _square0->SetEnabled(false);
                    _square1->SetEnabled(false);
                    _wave->SetEnabled(false);
                    _noise->SetEnabled(false);
                    _state.masterVolume = 0;
                    _state.outputEnable = 0;
                }

                _state.masterEnable = masterEnableNewVal;
            }
            break;
        }
        // Wave Sample Data (32 4-bit samples)
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            _wave->WriteWaveRam(addr - 0xFF30, val);
            break;
    }
}

void GameBoyApu::LoadState(std::ifstream &inState)
{
    inState.read((char *)&_state, sizeof(ApuState));
    inState.read((char *)&_pendingCycles, sizeof(_pendingCycles));
    inState.read((char *)&_cycleCount, sizeof(_pendingCycles));

    _square0->LoadState(inState);
    _square1->LoadState(inState);
    _wave->LoadState(inState);
    _noise->LoadState(inState);
}

void GameBoyApu::SaveState(std::ofstream &outState)
{
    outState.write((char *)&_state, sizeof(ApuState));
    outState.write((char *)&_pendingCycles, sizeof(_pendingCycles));
    outState.write((char *)&_cycleCount, sizeof(_pendingCycles));

    _square0->SaveState(outState);
    _square1->SaveState(outState);
    _wave->SaveState(outState);
    _noise->SaveState(outState);
}