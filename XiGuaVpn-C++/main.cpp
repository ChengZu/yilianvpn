#include <iostream>
#include "VpnServer.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif // _WIN32


int main(int argc, char **argv)
{
    if(argc > 1)
    {
        Config::PORT = atoi(argv[1]);
    }

    VpnServer vpnServer = VpnServer();

    int loopNum = 0;
    // Æô¶¯ÈÎÎñ
    while(Loop::taskNum())
    {
        Loop::executeTasks();
        if(loopNum > 1000)
        {
            loopNum = 0;
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1);
#endif // _WIN32
        }
        loopNum++;
    }

    printf("Vpn exit.\n");

    return 0;
}
