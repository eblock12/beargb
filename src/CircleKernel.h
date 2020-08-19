#pragma once

#include "GameBoy.h"
#include "IHostSystem.h"
#include <circle_stdlib_app.h>
#include <memory>

class CircleKernel : public CStdlibAppStdio, public IHostSystem
{
private:
    std::unique_ptr<GameBoy> _gameBoy;

    // unused
    TShutdownMode Run() override { return TShutdownMode::ShutdownHalt; }
public:
    CircleKernel();

    bool Initialize() override;
    HostExitCode RunApp(int argc, const char *argv[]) override;
};