#include "Kernel.h"

#if USE_SDL
#define EXIT_HALT	0
#define EXIT_REBOOT	1
#else
#include <circle/startup.h>
#endif

int main()
{
    Kernel kernel;
    if (!kernel.Initialize())
    {
#if !USE_SDL
        halt();
#endif
        return EXIT_HALT;
    }

    ShutdownMode shutdownMode = kernel.Run();
    switch (shutdownMode)
    {
        case ShutdownReboot:
#if !USE_SDL
            reboot ();
#endif
            return EXIT_REBOOT;

        case ShutdownHalt:
        default:
#if !USE_SDL
            halt();
#endif
            return EXIT_HALT;
    }
}