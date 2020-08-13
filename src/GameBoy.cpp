#include "GameBoy.h"

GameBoy::GameBoy(GameBoyModel model)
{
    _model = model;

    bool isCgb = (_model == GameBoyModel::GameBoyColor);
    _workRamSize = isCgb ? GameBoy::WorkRamSizeCgb : GameBoy::WorkRamSize;
    _videoRamSize = isCgb ? GameBoy::VideoRamSizeCgb : GameBoy::VideoRamSize;

    _workRam = new u8[_workRamSize];
    _highRam = new u8[GameBoy::HighRamSize];
    _videoRam = new u8[_videoRamSize];
    _oamRam = new u8[GameBoy::OamRamSize];
}

GameBoy::~GameBoy()
{
    delete[] _workRam;
    delete[] _highRam;
    delete[] _videoRam;
    delete[] _oamRam;
}