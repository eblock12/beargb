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
    std::vector<RomMenuItem>::iterator _selectedItem;
    std::filesystem::path _currentPath;

    void RefreshFileList();
public:
    OpenRomMenu(IHostSystem *host);
};