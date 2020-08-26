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

GameBoyCart *GameBoyCart::CreateFromRomFile(const char *filePath, GameBoy *gameBoy)
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

        switch (header.cartType)
        {
            case 0x00: // No mapper (32 KB ROM + 0 KB RAM)
                return new GameBoyCart(gameBoy, romData);
            case 0x01: // MBC1
            case 0x02: // MBC1+RAM
            case 0x03: // MBC1+RAM+Battery
                return new GameBoyCartMbc1(gameBoy, romData);
            case 0x05: // MBC2
            case 0x06: // MBC2+RAM+Battery
                return new GameBoyCartMbc2(gameBoy, romData);
            case 0x0F: // MBC3+Timer+Battery
            case 0x10: // MBC3+RAM+Timer+Battery
            case 0x11: // MBC3
            case 0x12: // MBC3+RAM
            case 0x13: // MBC3+RAM+Battery
                return new GameBoyCartMbc3(gameBoy, romData);
            case 0x19: // MBC5
            case 0x1A: // MBC5+RAM
            case 0x1B: // MBC5+RAM+Battery
            case 0x1C: // MBC5+Rumble
            case 0x1D: // MBC5+RAM+Rumble
            case 0x1E: // MBC5+RAM+Rumble+Battery
                return new GameBoyCartMbc5(gameBoy, romData);
            default:
                std::cout << "WARNING! Cart mapper type is unsupported" << std::endl;
                return new GameBoyCart(gameBoy, romData);
        }
    }
    else
    {
        std::cout << "Failed to open rom file: " << filePath << std::endl;

        // return a dummy cart
        u8 *romData = new u8[0x8000];
        memset(romData, 0, 0x8000);
        return new GameBoyCart(gameBoy, romData);
    }

    return nullptr;

}

void GameBoyCart::Reset()
{
    // TODO: Support bank swapping, for now just map in the first 32 KB of ROM always
    _gameBoy->MapMemory(_romData, 0x0000, _header.GetRomSize() & 0x8000, true /*readOnly*/);
}

void GameBoyCartMbc1::Reset()
{
    _ramGate = false;
    _bank1 = 1;
    _bank2 = 0;
    _mode = 0;

    // entire ROM area is writeable as register
    _gameBoy->MapRegisters(0x0000, 0x7FFF, false /*canRead*/, true /*canWrite*/);
}

void GameBoyCartMbc1::RefreshMemoryMap()
{
    // map the the ROM area
    _gameBoy->MapMemory(
        _romData + (_mode ? (_bank2 << 5) : 0) * PrgBankSize,
        0x0000,
        0x3FFF,
        true /*readOnly*/);
    _gameBoy->MapMemory(
        _romData + (_bank1 | (_bank2 << 5)) * PrgBankSize,
        0x4000,
        0x7FFF,
        true /*readOnly*/);

    // map the RAM area if allowed
    if (_ramGate)
    {
        u8 ramBank = _mode ? _bank2 : 0;
        _gameBoy->MapMemory(
            _cartRam + ramBank * RamBankSize,
            0xA000,
            0xBFFF,
            false /*readOnly*/);
        _gameBoy->UnmapRegisters(0xA000, 0xBFFF);
    }
    else
    {
        _gameBoy->UnmapMemory(0xA000, 0xBFFF);
        _gameBoy->MapRegisters(0xA000, 0xBFFF, true/*canRead*/, false/*canWrite*/);
    }
}

u8 GameBoyCartMbc1::ReadRegister(u16 addr)
{
    // this is returned when RAM is disabled
    return 0xFF;
}

void GameBoyCartMbc1::WriteRegister(u16 addr, u8 val)
{
    switch (addr & 0x6000)
    {
        case 0x0000: // RAMG: MBC1 RAM gate register
            _ramGate = ((val & 0x0F) == 0x0A);
            break;
        case 0x2000: // BANK1: MBC1 bank register 1
            _bank1 = val & 0x1F;
            if (_bank1 < 0)
            {
                // 0 is disallowed
                _bank1 = 1;
            }
            break;
        case 0x4000: // BANK2: MBC1 bank register 2
            _bank2 = val & 0x03;
            break;
        case 0x6000: // MODE: MBC1 mode register
            _mode = (val & 0x01);
            break;
    }

    RefreshMemoryMap();
}

void GameBoyCartMbc2::Reset()
{
    _ramGate = false;
    _romBank = 1;

    // lower ROM area is writeable as register
    _gameBoy->MapRegisters(0x0000, 0x3FFF, false /*canRead*/, true /*canWrite*/);
}

void GameBoyCartMbc2::RefreshMemoryMap()
{
    // map the the ROM area
    _gameBoy->MapMemory(
        _romData,
        0x0000,
        0x3FFF,
        true /*readOnly*/);
    _gameBoy->MapMemory(
        _romData + _romBank * PrgBankSize,
        0x4000,
        0x7FFF,
        true /*readOnly*/);

    // map the RAM area if allowed
    // TODO: RTC register mapping
    if (_ramGate)
    {
        for(int i = 0; i < 16; i++) // ram is mirrored 16 times in a row
        {
            _gameBoy->MapMemory(
                _cartRam,
                0xA000 + (0x200 * i),
                0xA1FF + (0x200 * i),
                false /*readOnly*/);
        }
        _gameBoy->MapRegisters(0xA000, 0xBFFF, false /*canRead*/, true /*canWrite*/);
    }
    else
    {
        _gameBoy->UnmapMemory(0xA000, 0xBFFF);
        _gameBoy->MapRegisters(0xA000, 0xBFFF, true/*canRead*/, false/*canWrite*/);
    }
}

u8 GameBoyCartMbc2::ReadRegister(u16 addr)
{
    // this is returned when RAM is disabled
    return 0xFF;
}

void GameBoyCartMbc2::WriteRegister(u16 addr, u8 val)
{
    // handle RAM writes as a register so the upper nibble can be chopped off
    if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        _cartRam[addr & 0x1FF] = val & 0x0F;
    }
    else
    {
        switch (addr & 0x100)
        {
            case 0x000: // RAM Enable
                _ramGate = ((val & 0x0F) == 0x0A);
                break;
            case 0x100: // ROM bank number
                _romBank = val & 0x0F;
                if (_romBank < 0)
                {
                    // 0 is disallowed
                    _romBank = 1;
                }
                break;
        }
    }

    RefreshMemoryMap();
}

void GameBoyCartMbc3::Reset()
{
    _ramGate = false;
    _romBank = 1;
    _ramBank = 0;

    // entire ROM area is writeable as register
    _gameBoy->MapRegisters(0x0000, 0x7FFF, false /*canRead*/, true /*canWrite*/);
}

void GameBoyCartMbc3::RefreshMemoryMap()
{
    // map the the ROM area
    _gameBoy->MapMemory(
        _romData,
        0x0000,
        0x3FFF,
        true /*readOnly*/);
    _gameBoy->MapMemory(
        _romData + _romBank * PrgBankSize,
        0x4000,
        0x7FFF,
        true /*readOnly*/);

    // map the RAM area if allowed
    // TODO: RTC register mapping
    if (_ramGate)
    {
        _gameBoy->MapMemory(
            _cartRam + _ramBank * RamBankSize,
            0xA000,
            0xBFFF,
            false /*readOnly*/);
        _gameBoy->UnmapRegisters(0xA000, 0xBFFF);
    }
    else
    {
        _gameBoy->UnmapMemory(0xA000, 0xBFFF);
        _gameBoy->MapRegisters(0xA000, 0xBFFF, true/*canRead*/, false/*canWrite*/);
    }
}

u8 GameBoyCartMbc3::ReadRegister(u16 addr)
{
    // TODO: RTC registers

    // this is returned when RAM is disabled
    return 0xFF;
}

void GameBoyCartMbc3::WriteRegister(u16 addr, u8 val)
{
    // TODO: RTC registers

    switch (addr & 0x6000)
    {
        case 0x0000: // RAMG: MBC3 RAM gate register
            _ramGate = ((val & 0x0F) == 0x0A);
            break;
        case 0x2000: // BANK1: MBC3 bank register 1
            _romBank = val;
            if (_romBank < 0)
            {
                // 0 is disallowed
                _romBank = 1;
            }
            break;
        case 0x4000: // BANK2: MBC3 bank register 2
            _ramBank = val & 0x0F;
            break;
    }

    RefreshMemoryMap();
}

void GameBoyCartMbc5::Reset()
{
    _ramGate = false;
    _romBank = 1;
    _ramBank = 0;

    _gameBoy->MapRegisters(0x0000, 0x5FFF, false /*canRead*/, true /*canWrite*/);
}

void GameBoyCartMbc5::RefreshMemoryMap()
{
    // map the the ROM area
    _gameBoy->MapMemory(
        _romData,
        0x0000,
        0x3FFF,
        true /*readOnly*/);
    _gameBoy->MapMemory(
        _romData + _romBank * PrgBankSize,
        0x4000,
        0x7FFF,
        true /*readOnly*/);

    // map the RAM area if allowed
    if (_ramGate)
    {
        _gameBoy->MapMemory(
            _cartRam + _ramBank * RamBankSize,
            0xA000,
            0xBFFF,
            false /*readOnly*/);
        _gameBoy->UnmapRegisters(0xA000, 0xBFFF);
    }
    else
    {
        _gameBoy->UnmapMemory(0xA000, 0xBFFF);
        _gameBoy->MapRegisters(0xA000, 0xBFFF, true/*canRead*/, false/*canWrite*/);
    }
}

u8 GameBoyCartMbc5::ReadRegister(u16 addr)
{
    // this is returned when RAM is disabled
    return 0xFF;
}

void GameBoyCartMbc5::WriteRegister(u16 addr, u8 val)
{
     switch (addr & 0x7000)
    {
        case 0x0000: // RAM Enable
        case 0x1000:
            _ramGate = (val == 0x0A);
            break;

        case 0x2000: // Low 8 bits of ROM Bank Number
            _romBank = val | (_romBank & 0x100);
            break;

        case 0x3000: // High bit of ROM Bank Number
            _romBank = (_romBank & 0xFF) | ((val & 0x01) << 8);
            break;

        case 0x4000: // RAM Bank Number
        case 0x5000:
            _ramBank = val & 0x0F;
            break;
    }

    RefreshMemoryMap();
}