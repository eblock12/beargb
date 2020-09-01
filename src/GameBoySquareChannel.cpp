#include "GameBoySquareChannel.h"
#include "GameBoyApu.h"
#include <iostream>

constexpr s8 GameBoySquareChannel::DutyTable[4][8];

GameBoySquareChannel::GameBoySquareChannel(GameBoyApu *apu)
{
    _apu = apu;
    _state = {};
}

GameBoySquareChannel::~GameBoySquareChannel()
{
}

void GameBoySquareChannel::Execute(u32 cycles)
{
    _state.timer -= cycles;

    if (_state.enabled)
    {
        _state.output = GameBoySquareChannel::DutyTable[_state.dutyCycleSelect][_state.dutyCyclePosition] * _state.volume;
    }
    else
    {
        _state.output = 0;
    }

    //std::cout << int(_state.frequency) << std::endl;

    // check if frequency timer is triggered
    if (_state.timer == 0)
    {
        // reload timer
        _state.timer = (2048 - _state.frequency) * 4;

        // advance to next position in duty cycle
        _state.dutyCyclePosition = (_state.dutyCyclePosition + 1) & 0x07;
    }
}

void GameBoySquareChannel::TickCounter()
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

void GameBoySquareChannel::TickFrequencyEnvelope()
{
    if (_state.sweepEnable &&
        (_state.sweepTimer > 0) &&
        (_state.sweepLength > 0))
    {
        _state.sweepTimer--;

        if (_state.sweepTimer == 0)
        {
            _state.sweepTimer = _state.sweepLength;

            u16 newSweepFrequency = CalculateSweepFrequency();

            if ((_state.sweepShift > 0) && (newSweepFrequency < 2048))
            {
                _state.frequency = _state.sweepFrequency;
                _state.sweepFrequency = newSweepFrequency;

                newSweepFrequency = CalculateSweepFrequency();

                if (newSweepFrequency >= 2048)
                {
                    _state.sweepFrequency = _state.enabled = false;
                }
            }
            else
            {
                _state.sweepFrequency = _state.enabled = false;
            }
        }
    }
}

void GameBoySquareChannel::TickVolumeEnvelope()
{
    if (!_state.envelopeHalted && (_state.envelopeTimer > 0))
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
            else
            {
                // nothing to do
                _state.envelopeHalted = true;
            }

            _state.envelopeTimer = _state.envelopeLength;
        }
    }
}

u16 GameBoySquareChannel::CalculateSweepFrequency()
{
    u16 result = (_state.sweepFrequency >> _state.sweepShift);

    if(_state.sweepDecrease)
    {
        return _state.sweepFrequency - result;
    }
    else
    {
        return _state.sweepFrequency + result;
    }
}

u8 GameBoySquareChannel::ReadRegister(u16 addr)
{
    switch (addr)
    {
        case 0: // Sweep Envelope (FF10) - Square0 only
            return
                (_state.sweepShift & 0x07) |
                (_state.sweepDecrease ? 0x08 : 0x00) |
                ((_state.sweepLength & 0x07) << 4);

        case 1: // Duty Pattern & Length (FF11/FF16)
            return (_state.dutyCycleSelect & 0x03) << 7;

        case 2: // Volume Envelope (FF12/FF17)
            return (_state.envelopeLength & 0x07) |
                (_state.envelopeIncrease ? 0x08 : 0) |
                ((_state.envelopeVolume & 0x0F) << 4);
        case 3:
            return 0xFF;
    }

    return 0xFF;
}

void GameBoySquareChannel::WriteRegister(u16 addr, u8 val)
{
    switch (addr)
    {
        case 0:  // Sweep Envelope $FF10) - Square0 only
            _state.sweepShift = val & 0x07;
            _state.sweepDecrease = (val & 0x08) != 0;
            _state.sweepLength = (val >> 4) & 0x07;
            break;

        case 1: // Duty Pattern & Length (FF10/FF16)
            _state.length = 64 - (val & 0x3F); // length = (64 -t1) x (1/256sec)
            _state.dutyCycleSelect = (val >> 6) & 0x03;
            break;

        case 2: // Volume Envelope (FF12/FF17)
            _state.envelopeLength = val & 0x07;
            _state.envelopeIncrease = (val & 0x08) != 0;
            _state.envelopeVolume = (val >> 4) & 0x0F;

            if ((val & 0xF8) == 0) // volume set to 0
            {
                _state.enabled = false;
            }
            break;

        case 3: // Frequency Low Byte (FF13/FF18)
            _state.frequency = (_state.frequency & 0x700) | val;
            //std::cout << "freq=" << std::dec << int(_state.frequency) << std::endl;
            break;

        case 4: // Frequency (upper 3 bits), initial/counter (FF14/FF19)
            _state.frequency = (_state.frequency & 0xFF) | ((val & 0x07) << 8);
            _state.lengthEnable = (val & 0x40) != 0;
            if (val & 0x80) // initialize/restarts channel
            {
                // reference: https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Trigger_Event

                _state.enabled = _state.envelopeIncrease || _state.envelopeVolume;

                if (_state.length == 0)
                {
                    _state.length = 64; // if reusing this, change to 256 for wave channel
                    _state.lengthEnable = false;
                }

                // reset frequency timer
                _state.timer = (2048 - _state.frequency) * 4;

                // reset volume envelope timer
                _state.envelopeTimer = _state.envelopeLength;
                _state.envelopeHalted = false;

                // load internal volume from envelope volume
                _state.volume = _state.envelopeVolume;

                // reload shadow register for sweep frequency
                _state.sweepFrequency = _state.frequency;

                // reload sweep timer
                _state.sweepTimer = _state.sweepLength;

                // set sweep enable if sweep length or shift is set
                _state.sweepEnable = _state.sweepLength || _state.sweepShift;

                if (_state.sweepShift)
                {
                    // immediately update frequency calculation
                    _state.sweepFrequency = CalculateSweepFrequency();
                    if (_state.sweepFrequency > 2047)
                    {
                        // check for overflow
                        _state.sweepEnable = _state.enabled = false;
                    }
                }
            }
            break;
    }
}