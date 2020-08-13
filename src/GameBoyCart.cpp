#include "GameBoyCart.h"

GameBoyCart::GameBoyCart(u8 *romData)
{
    _romData = romData;
}

GameBoyCart::~GameBoyCart()
{
    if (_romData != nullptr)
    {
        delete[] _romData;
    }
}

GameBoyCart *GameBoyCart::LoadFromRomFile(std::string filePath)
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
                cgbSupport = "Unsupported";
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
        std::cout << "CGB Support: " << cgbSupport << std::endl;
        std::cout << "SGB Support: " << (header.sgbFlag == 0x03 ? "Supported" : "Unsupported") << std::endl;
        
        return new GameBoyCart(romData);
    }

    return nullptr; 
}