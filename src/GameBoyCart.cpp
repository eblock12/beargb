#include "GameBoyCart.h"
#include "GameBoy.h"
#include <string.h>
#include <fstream>
#include <iostream>

GameBoyCart::GameBoyCart(GameBoy *gameBoy, u8 *romData)
{
    _gameBoy = gameBoy;
    _romData = romData;
    memcpy(&_header, romData + HeaderOffset, sizeof(GameBoyCartHeader));
}

GameBoyCart::~GameBoyCart()
{
    if (_romData != nullptr)
    {
        delete[] _romData;
    }
}

GameBoyCart *GameBoyCart::CreateFromRomFile(const std::string &filePath, GameBoy *gameBoy)
{
    std::ifstream romStream(filePath, std::ios::in | std::ios::binary);
    if (romStream.good())
    {
        std::cout << "Opened ROM file: " << filePath << std::endl;

        // get file size by seeking to EOF and check offset
        romStream.seekg(0, std::ios::end);
        u32 fileSize = (u32)romStream.tellg();
        romStream.seekg(0, std::ios::beg);

        // read entire stream info buffer
        u8 *romData = new u8[fileSize];
        romStream.read((char *)romData, fileSize);

        GameBoyCartHeader header;
        memcpy(&header, romData + HeaderOffset, sizeof(GameBoyCartHeader));

        std::string cgbSupport = "Invalid";
        switch (header.cgbFlag & 0xC0)
        {
            case 0x00:
                cgbSupport = "No";
                break;
            case 0x80:
                cgbSupport = "Optional";
                break;
            case 0xC0:
                cgbSupport = "Required";
                break;
        }

        std::cout << "Title: " << header.title << std::endl;
        std::cout << "Type: " << header.GetCartTypeName() << std::endl;
        std::cout << "ROM Size: " << std::to_string(header.GetRomSize() / 1024) << " KB" << std::endl;
        std::cout << "RAM Size: " << std::to_string(header.GetRamSize() / 1024) << " KB" << std::endl;
        std::cout << "Battery: " << (header.HasBattery() ? "Yes" : "No") << std::endl;
        std::cout << "CGB Support: " << cgbSupport << std::endl;
        std::cout << "SGB Support: " << (header.sgbFlag == 0x03 ? "Yes" : "No") << std::endl;

        return new GameBoyCart(gameBoy, romData);
    }

    return nullptr;
}

void GameBoyCart::Reset()
{
    // TODO: Support bank swapping, for now just map in the first 32 KB of ROM always
    _gameBoy->MapMemory(_romData, 0x0000, _header.GetRomSize() & 0x8000, true /*readOnly*/);
}