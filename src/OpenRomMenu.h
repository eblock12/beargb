#pragma once

#include "IHostSystem.h"

#include <filesystem>
#include <string>
#include <vector>

using namespace std::string_literals;

enum class RomMenuItemType
{
    RomFile,
    Directory
};

struct RomMenuItem
{
    std::string text;
    std::filesystem::path path;
    RomMenuItemType type;
};

class OpenRomMenu
{
private:
    IHostSystem *_host;

    std::vector<RomMenuItem> _romList;
    std::vector<RomMenuItem>::iterator _topItem;
    std::vector<RomMenuItem>::iterator _bottomItem;
    std::vector<RomMenuItem>::iterator _selectedItem;
    std::filesystem::path _currentPath;

    u16 _lastInputState;
    unsigned _inputHeldFrames;

    u32 *_pixelBuffer;

    void DrawFileList();
    void DrawString(const std::string &str, int x, int y);
    void InvertRect(int x, int y, int width, int height);
    void RefreshFileList();
    void ScrollIntoView(std::vector<RomMenuItem>::iterator item);
    void ProcessInput();
    void ProcessButtonPress(u16 inputState);
public:
    OpenRomMenu(IHostSystem *host);
    ~OpenRomMenu();

    u32 *GetPixelBuffer() { return _pixelBuffer; }
    void RunFrame();
};