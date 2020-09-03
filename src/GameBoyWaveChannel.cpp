#include "GameBoyWaveChannel.h"

GameBoyWaveChannel::GameBoyWaveChannel(GameBoyApu *apu)
{
    _apu = apu;
    _state = {};
}

GameBoyWaveChannel::~GameBoyWaveChannel()
{
}
#include <iostream>
void GameBoyWaveChannel::Execute(u32 cycles)
{
    if (_state.enabled && _state.volume)
    {
        _state.output = _state.waveBuffer >> (_state.volume - 1);
       // std::cout << "wave output=" << std::dec << int(_state.output) << std::endl;
    }
    else
    {
        _state.output = 0;
    }

    _state.timer -= cycles;
    if (_state.timer == 0)
    {
        // "The wave channel's frequency timer period is set to (2048-frequency)*2"
        _state.timer = (2048 - _state.frequency) * 2;

        _state.position++;
        _state.position &= 0x1F; // loops

        // alternate between upper and lower nibbles (4-bit samples)
        if (_state.position & 0x01)
        {
            _state.waveBuffer = _state.waveRam[_state.position >> 1] & 0x0F;
        }
        else
        {
            _state.waveBuffer = _state.waveRam[_state.position >> 1] >> 4;
        }
    }
}

void GameBoyWaveChannel::TickCounter()
{
    if (_state.lengthEnable && (_state.length > 0))
    {
        _state.length--;

        if (_state.length == 0)
        {
            _state.enabled = false;
        }
    }
}

u8 GameBoyWaveChannel::ReadRegister(u16 addr)
{
    switch (addr)
    {
        case 0: // Sound on/off (FF1A)
            return _state.dacPowered ? 0x80 : 0;

        case 2: // Output level (FF1C)
            return _state.volume << 5;

        case 4: // Length trigger enable
            return _state.lengthEnable ? 0x40 : 0;
    }

    return 0xFF; // open bus
}

void GameBoyWaveChannel::WriteRegister(u16 addr, u8 val)
{
    switch (addr)
    {
        case 0: // Sound on/off (FF1A)
            _state.dacPowered = (val & 0x80) != 0;
            _state.enabled = _state.enabled & _state.dacPowered;
            break;

        case 1: // Length data (FF1B)
            _state.length = 256 - val;
            break;

        case 2: // Output level (FF1C)
            _state.volume = (val >> 5) & 0x3;
            break;

        case 3: // Frequency data low-byte (FF1D)
            _state.frequency = (_state.frequency & 0x700) | val;
            break;

        case 4: // Trigger/initialize/frequency MSB (FF1E)
            _state.frequency = (_state.frequency & 0xFF) | ((val & 0x07) << 8);
            _state.lengthEnable = (val & 0x40) != 0;

            if (val & 0x80) // initialize/restarts channel
            {
                // enabled as long as the DAC is powered
                _state.enabled = _state.dacPowered;

                if (_state.length == 0)
                {
                    _state.length = 256;
                    _state.lengthEnable = false;
                }

                _state.timer = (2048 - _state.frequency) * 2;
                _state.position = 0;
            }

            break;
    }
}

u8 GameBoyWaveChannel::ReadWaveRam(u8 addr)
{
    return _state.waveRam[addr & 0x0F];
}

void GameBoyWaveChannel::WriteWaveRam(u8 addr, u8 val)
{
    _state.waveRam[addr & 0x0F] = val;
}

void GameBoyWaveChannel::LoadState(std::ifstream &inState)
{
    inState.read((char *)&_state, sizeof(WaveChannelState));
}

void GameBoyWaveChannel::SaveState(std::ofstream &outState)
{
    outState.write((char *)&_state, sizeof(WaveChannelState));
}