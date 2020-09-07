#include "GameBoyNoiseChannel.h"
#include <iostream>

GameBoyNoiseChannel::GameBoyNoiseChannel(GameBoyApu *apu)
{
    _apu = apu;
    _state = {};
}

GameBoyNoiseChannel::~GameBoyNoiseChannel()
{
}

void GameBoyNoiseChannel::SetEnabled(bool val)
{
    if (!val)
    {
        // zero all regs except length
        u8 length = _state.length;
        _state = { 0 };
        _state.length = length;
    }

    _state.enabled = val;
}

void GameBoyNoiseChannel::Execute(u32 cycles)
{
    // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
    // "Using a noise channel clock shift of 14 or 15 results in the LFSR receiving no clocks.""
	if (_state.shiftFrequency >= 14) {
		return;
	}

	if (_state.enabled)
    {
		_state.output = ((_state.shiftRegister & 0x01) ^ 0x01) * _state.volume;

        //std::cout << std::dec << "output=" << int(_state.output) << std::endl;
        //std::cout << std::dec << "shiftRegister=" << int(_state.shiftRegister) << std::endl;
	}
    else
    {
		_state.output = 0;
	}

    _state.timer -= cycles;
    if (_state.timer == 0)
    {
        ResetTimer();

        // When clocked by the frequency timer, the low two bits (0 and 1) are XORed,
        // all bits are shifted right by one, and the result of the XOR is put into
        // the now-empty high bit.
        u16 allShiftedRight = _state.shiftRegister >> 1;
	    u8 lowXorBits = (_state.shiftRegister & 0x01) ^ (allShiftedRight & 0x01);
		_state.shiftRegister = (lowXorBits << 14) | allShiftedRight;

        if (_state.useShortStep)
        {
			// If width mode is 1 (NR43), the XOR result is ALSO put into bit 6 AFTER the shift, resulting in a 7-bit LFSR.
			_state.shiftRegister &= ~0x40;
			_state.shiftRegister |= (lowXorBits << 6);
        }
    }
}

void GameBoyNoiseChannel::TickCounter()
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

void GameBoyNoiseChannel::TickVolumeEnvelope()
{
    if (_state.envelopeTimer > 0)
    {
        _state.envelopeTimer--;

        if (_state.envelopeTimer == 0)
        {
            if (_state.envelopeIncrease && _state.volume < 0x0F)
            {
                _state.volume++;
            }
            else if (!_state.envelopeIncrease && _state.volume > 0)
            {
                _state.volume--;
            }

            _state.envelopeTimer = _state.envelopeLength;
        }
    }
}

void GameBoyNoiseChannel::ResetTimer()
{
	if (_state.dividingRatio == 0)
    {
		_state.timer = 8 << _state.shiftFrequency;
	}
    else
    {
		_state.timer = (16 * _state.dividingRatio) << _state.shiftFrequency;
	}
}

u8 GameBoyNoiseChannel::ReadRegister(u16 addr)
{
    switch (addr)
    {
        case 0: // Noise length (FF20)
            return 0xFF; // open bus
        
        case 1: // Noise volume envelope (FF21)
            return (_state.envelopeLength & 0x07) |
                (_state.envelopeIncrease ? 0x08 : 0) |
                ((_state.envelopeVolume & 0x0F) << 4);

        case 2: // Noise polynomial counter (FF22)
            return _state.dividingRatio |
                (_state.useShortStep ? 0x08 : 0) |
                (_state.shiftFrequency << 4);

        case 3: // Noise initialize/counter (FF23)
            return 0xBF | // open bits
                (_state.lengthEnable ? 0x40 : 0);
    }

    return 0xFF; // open bus
}

void GameBoyNoiseChannel::WriteRegister(u16 addr, u8 val)
{
    switch (addr)
    {
        case 0: // Noise length (FF20)
            _state.length = 64 - (val & 0x3F); 
            break;
        
        case 1: // Noise volume envelope (FF21)
            _state.envelopeLength = val & 0x07;
            _state.envelopeIncrease = (val & 0x08) != 0;
            _state.envelopeVolume = (val >> 4) & 0x0F;

            if ((val & 0xF8) == 0) // volume set to 0
            {
                _state.enabled = false;
            }
            break;

        case 2: // Noise polynomial counter (FF22)
            _state.dividingRatio = val & 0x07;
            _state.useShortStep = (val & 0x08) != 0;
            _state.shiftFrequency = (val >> 4) & 0x0F;
            break;

        case 3: // Noise initial/counter (FF23)
            _state.lengthEnable = (val & 0x40) != 0;
            if (val & 0x80) // initialize/restarts channel
            {
                _state.enabled = _state.envelopeIncrease || _state.envelopeVolume;

                if (_state.length == 0)
                {
                    _state.length = 64; // if reusing this, change to 256 for wave channel
                    _state.lengthEnable = false;
                }

                // reset the frequency timer
                ResetTimer();

                // initially all bits in the shift register are enabled (15-bit)
                _state.shiftRegister = 0x7FFF;

                // reset volume envelope timer
                _state.envelopeTimer = _state.envelopeLength;

                // load internal volume from envelope volume
                _state.volume = _state.envelopeVolume;
            }
            break;
    }
}

void GameBoyNoiseChannel::LoadState(std::ifstream &inState)
{
    inState.read((char *)&_state, sizeof(NoiseChannelState));
}

void GameBoyNoiseChannel::SaveState(std::ofstream &outState)
{
    outState.write((char *)&_state, sizeof(NoiseChannelState));
}