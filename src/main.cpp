#if USE_SDL
    #include "SdlApp.h"
#else
    #include "CircleKernel.h"
    #include <circle/startup.h>
#endif

int main(int argc, const char *argv[])
{
#ifdef USE_SDL
        SdlApp sdlApp;
        IHostSystem *host = &sdlApp;
#else
        CircleKernel circleKernel;
        IHostSystem *host = &circleKernel;
#endif
    
    
}