#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include "shared.h"

struct GameBoyCartHeader
{
    // reference: https://gbdev.gg8.se/wiki/articles/The_Cartridge_Header

    // starting at 0x0134
    u8 title[11];
    u8 oemCode[4];
    u8 cgbFlag;
    u8 licenseeCode[2];
    u8 sgbFlag;
    u8 cartType;
    u8 romSize;
    u8 ramSize;
    u8 region;
    u8 oldLicenseeCode;
    u8 romVersion;
    u8 checksum;
    u8 globalChecksum[2];

    std::string GetCartTypeName()
    {
        switch (cartType)
        {
            case 0x00:
                return "ROM Only";
            case 0x01:
                return "MBC1";
            case 0x02:
                return "MBC1+RAM";
            case 0x03:
                return "MBC1+RAM+Battery";
            case 0x05:
                return "MBC2";
            case 0x06:
                return "MBC2+Battery";
            case 0x08:
                return "ROM+RAM";
            case 0x09:
                return "ROM+RAM+Battery";
            case 0x0B:
                return "MMM01";
            case 0x0C:
                return "MMM01+RAM";
            case 0x0D:
                return "MMM01+RAM+Battery";
            case 0x0F:
                return "MBC3+Timer+Battery";
            case 0x10:
                return "MBC3+Timer+RAM+Battery";
            case 0x11:
                return "MBC3";
            case 0x12:
                return "MBC3+RAM";
            case 0x13:
                return "MBC3+RAM+Battery";
            case 0x19:
                return "MBC5";
            case 0x1A:
                return "MBC5+RAM";
            case 0x1B:
                return "MBC5+RAM+Battery";
            case 0x1C:
                return "MBC5+Rumble";
            case 0x1D:
                return "MBC5+Rumble+RAM";
            case 0x1E:
                return "MBC5+Rumble+RAM+Battery";
            case 0x20:
                return "MBC6";
            case 0x22:
                return "MBC7+Sensor+Rumble+RAM+Battery";
            case 0xFC:
                return "Pocket Camera";
            case 0xFD:
                return "Bandai TAMA5";
            case 0xFE:
                return "Huc3";
            case 0xFF:
                return "Huc1+RAM+Battery";
            default:
                return "Unknown";
        }
    }

    u32 GetRomSize()
    {
        return 0x8000 << romSize;
    }

    u32 GetRamSize()
    {
        switch (cartType)
        {
            case 0x05: // MBC2
            case 0x06: // MBC2+Battery
                return 0x200; 
        }

        switch (ramSize)
        {
            case 0:
                return 0; // no cart RAM
            case 1:
                return 0x800; // 2 KB 
            case 2:
                return 0x2000; // 8 KB
            case 3:
                return 0x8000; // 32 KB
            case 4:
                return 0x20000; // 128 KB
            case 5:
                return 0x10000; // 64 KB
        }

        return 0;
    }

    bool HasBattery()
    {
        switch (cartType)
        {
            case 0x03: // MBC1+RAM+BATTERY
            case 0x06: // MBC2+BATTERY
            case 0x09: // ROM+RAM+BATTERY
            case 0x0D: // MMM01+RAM+BATTERY
            case 0x0F: // MBC3+TIMER+BATTERY
            case 0x10: // MBC3+TIMER+RAM+BATTERY
            case 0x13: // MBC3+RAM+BATTERY
            case 0x1B: // MBC5+RAM+BATTERY
            case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
            case 0x22: // MBC7+SENSOR+RUMBLE+RAM+BATTERY
            case 0xFF: // HuC1+RAM+BATTERY
                return true;
        }

        return false;
    }
};

class GameBoyCart
{
private:
    u8 *_romData;

    GameBoyCart(u8 *romData);
public:
    static constexpr int HeaderOffset = 0x134;

    ~GameBoyCart();
    static GameBoyCart *LoadFromRomFile(std::string filePath);
};