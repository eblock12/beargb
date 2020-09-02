#include "OpenRomMenu.h"
#include "font8x8_basic.h"

#include <dirent.h>
#include <algorithm>
#include <iostream>
#include <set>
#include <string.h>

constexpr unsigned ScreenWidth = 160;
constexpr unsigned ScreenHeight = 144;
constexpr unsigned ScreenStride = ScreenWidth * sizeof(u32);
constexpr unsigned ItemsPerScreen = ScreenHeight / 8 - 1;

constexpr unsigned KeyRepeatStartFrames = 30; // 500 ms until repeat starts
constexpr unsigned KeyRepeatInterval = 2; // every 10 frames (100 ms)

OpenRomMenu::OpenRomMenu(IHostSystem *host)
{
    _host = host;
    _pixelBuffer = new u32[ScreenWidth * ScreenHeight];
    memset(_pixelBuffer, 0, ScreenWidth * ScreenHeight * sizeof(u32));
    _currentPath = std::filesystem::path("roms"s);

    RefreshFileList();
}

OpenRomMenu::~OpenRomMenu()
{
    delete[] _pixelBuffer;
}

void OpenRomMenu::DrawFileList()
{
    int y = 0;

    auto fileIterator = _topItem;
    for (auto fileIterator = _topItem; fileIterator <= _bottomItem; fileIterator++)
    {
        RomMenuItem item = *fileIterator;
        DrawString(item.text, 0, y);

        if (fileIterator == _selectedItem)
        {
            InvertRect(0, y, ScreenWidth, 8);
        }
        y+= 8;
    }
}

void OpenRomMenu::DrawString(const std::string &str, int x, int y)
{
    for (const char &strChar : str)
    {
        char c = strChar;
        if (c < 0 || c > 127)
        {
            c = '?';
        }

        const u8 *bitmap = font8x8_basic[c];

        for (int bmY = 0; bmY < 8; bmY++)
        {
            for (int bmX = 0; bmX < 8; bmX++)
            {
                u8 pixel = bitmap[bmY] & (1 << bmX);

                int px = x + bmX;
                int py = y + bmY;

                if ((px >= 0) && (py >= 0) && (px < ScreenWidth) && (py < ScreenHeight))
                {
                    _pixelBuffer[(y + bmY) * ScreenWidth + x + bmX] = (u32)pixel * 0xFFFFFF00;
                }
            }
        }

        x += 8;
    }
}

void OpenRomMenu::InvertRect(int x, int y, int width, int height)
{
    for (int py = y; py < (y + height); py++)
    for (int px = x; px < (x + width); px++)
    {
        if ((px >= 0) && (py >= 0) && (px < ScreenWidth) && (py < ScreenHeight))
        {
            _pixelBuffer[py * ScreenWidth + px] ^= 0xFFFFFF00;
        }
    }
}

void OpenRomMenu::RefreshFileList()
{
    std::set<std::string> allExts { ".gb"s, ".gbc"s };

    std::vector<RomMenuItem> dirList;
    std::vector<RomMenuItem> fileList;
/*
    // create special entry back to the parent path if we're not already the root
    if (_currentPath.root_path() != _currentPath)
    {
        RomMenuItem parentItem {
            .text = "<..>",
            .path = _currentPath.parent_path(),
            .type = RomMenuItemType::Directory
        };
        dirList.push_back(parentItem);
    }
*/

    DIR * const dir = opendir(_currentPath.string().c_str());
    if (dir)
    {
        for (;;)
        {
            struct dirent const *const dp = readdir(dir);
            if (dp)
            {
                auto entryPath = std::filesystem::path(dp->d_name);

                // TODO: Identify directory types under FatFS driver

                if (allExts.count(entryPath.extension().string()) > 0)
                {
                    RomMenuItem newItem {
                        .text = entryPath.filename().string(),
                        .path = _currentPath / entryPath,
                        .type = RomMenuItemType::RomFile
                    };
                    fileList.push_back(newItem);
                }
            }
            else
            {
                // no more entries or error, leave loop
                break;
            }
        }
    }

    std::sort(fileList.begin(), fileList.end(), [](const RomMenuItem a, const RomMenuItem b)
    {
        return a.text < b.text;
    });

    // merge directories and files together
    _romList.clear();
    _romList.reserve(dirList.size() + fileList.size());
    _romList.insert(_romList.end(), dirList.begin(), dirList.end());
    _romList.insert(_romList.end(), fileList.begin(), fileList.end());

    _selectedItem = _romList.begin();
    _topItem = _romList.begin();
    _bottomItem = _topItem + ItemsPerScreen;
    if (_bottomItem > _romList.end())
    {
        _bottomItem = _romList.end() - 1;
    }

    std::cout << std::string(10, '-') << std::endl;
    for (auto romEntry : _romList)
    {
        std::cout << romEntry.text << std::endl;
    }
    std::cout << std::string(10, '-') << std::endl;
}

void OpenRomMenu::ProcessInput()
{
    u16 inputState = 0;

    inputState |= _host->IsButtonPressed(HostButton::Down) ? HostButton::Down : 0;
    inputState |= _host->IsButtonPressed(HostButton::Up) ? HostButton::Up : 0;
    inputState |= _host->IsButtonPressed(HostButton::Start) ? HostButton::Start : 0;

    if (inputState != _lastInputState)
    {
        _lastInputState = inputState;
        _inputHeldFrames = 0;
        ProcessButtonPress(inputState);
    }
    else
    {
        _inputHeldFrames++;

        if (_inputHeldFrames >= KeyRepeatStartFrames)
        {
            if (_inputHeldFrames % KeyRepeatInterval == 0)
            {
                ProcessButtonPress(inputState);
            }
        }
    }
}

void OpenRomMenu::ProcessButtonPress(u16 inputState)
{
    if (inputState & HostButton::Down)
    {
        _selectedItem++;
        if (_selectedItem >= _romList.end())
        {
            _selectedItem = _romList.begin();
        }
    }
    else if (inputState & HostButton::Up)
    {
        _selectedItem--;
        if (_selectedItem < _romList.begin())
        {
            _selectedItem = _romList.end() - 1;
        }
    }
    else if (inputState & HostButton::Start)
    {
        _host->LoadRomFile(_selectedItem->path.string().c_str());
    }
}

void OpenRomMenu::RunFrame()
{
    ProcessInput();

    ScrollIntoView(_selectedItem);

    memset(_pixelBuffer, 0, ScreenHeight * ScreenStride);

    DrawFileList();

    _host->PushVideoFrame(_pixelBuffer);
}

void OpenRomMenu::ScrollIntoView(std::vector<RomMenuItem>::iterator item)
{
    if (_selectedItem < _topItem)
    {
        _topItem = _selectedItem;
        _bottomItem = _topItem + ItemsPerScreen;
    }
    else if (_selectedItem > _bottomItem)
    {
        _bottomItem = _selectedItem;
        _topItem = _bottomItem - ItemsPerScreen;
    }

    if (_bottomItem >= _romList.end())
    {
        _bottomItem = _romList.end() - 1;
    }
    if (_topItem < _romList.begin())
    {
        _topItem = _romList.begin();
    }
}