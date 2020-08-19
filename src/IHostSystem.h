#pragma once

enum class HostExitCode
{
    Success = 0,
    Failure = 1,
};

class IHostSystem
{
    virtual bool Initialize() = 0;
    virtual HostExitCode RunApp(int argc, const char *argv[]);
};