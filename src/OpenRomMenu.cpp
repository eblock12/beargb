#include "OpenRomMenu.h"

#include <algorithm>
#include <iostream>
#include <set>

OpenRomMenu::OpenRomMenu(IHostSystem *host)
{
    _host = host;
    _currentPath = std::filesystem::path("."s);

    RefreshFileList();
}

void OpenRomMenu::RefreshFileList()
{
    std::set<std::string> allExts { ".gb", ".gbc" };

    std::filesystem::directory_iterator iter(_currentPath);
    std::filesystem::directory_iterator end;

    _romList.clear();

    std::vector<RomMenuItem> dirList;
    std::vector<RomMenuItem> fileList;

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

    while (iter != end)
    {
        auto path = iter->path();

        // is this file entry a supported ROM file or directory?
        if (iter->is_directory())
        {
            // skip over hidden directories (starting with a ".")
            if (path.filename().string().rfind('.', 0) == std::string::npos)
            {
                RomMenuItem newItem {
                    .text = "<"s + path.filename().string() + ">"s,
                    .path = path,
                    .type = RomMenuItemType::Directory
                };
                dirList.push_back(newItem);
            }
        }
        else if (iter->is_regular_file())
        {
            std::string fileExtension = path.extension().string();
            if (allExts.count(fileExtension) > 0)
            {
                RomMenuItem newItem {
                    .text = path.filename().string(),
                    .path = path,
                    .type = RomMenuItemType::RomFile
                };
                fileList.push_back(newItem);
            }
        }
        else
        {
            // Found some unsupported type
        }

        std::error_code errorCode;
        iter.increment(errorCode);
        if (errorCode)
        {
            std::cerr << errorCode.message() << std::endl;
        }
    }

    // merge directories and files together
    _romList.clear();
    _romList.reserve(dirList.size() + fileList.size());
    _romList.insert(_romList.end(), dirList.begin(), dirList.end());
    _romList.insert(_romList.end(), fileList.begin(), fileList.end());

    std::cout << std::string(40, '-') << std::endl;
    for (auto romEntry : _romList)
    {
        std::cout << romEntry.text << std::endl;
    }
    std::cout << std::string(40, '-') << std::endl;
}