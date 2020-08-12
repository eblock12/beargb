#include "kernel.h"
#include <circle/startup.h>

int main()
{
    Kernel kernel;
    if (!kernel.Initialize())
    {
        halt();
        return EXIT_HALT;
    }

    ShutdownMode shutdownMode = kernel.Run();
    switch (shutdownMode)
    {
        case ShutdownReboot:
            reboot ();
            return EXIT_REBOOT;

        case ShutdownHalt:
        default:
            halt();
            return EXIT_HALT;
    }
}