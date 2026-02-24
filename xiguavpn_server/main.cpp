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

	VpnServer vpnServer;

	int loopCount = 0;
	// Æô¶¯ÈÎÎñ
	while(task_loop() > 0)
	{
		if(loopCount > 1000)
		{
			loopCount = 0;
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif // _WIN32
		}
		loopCount++;
	}

	printf("Vpn exit.\n");

	return 0;
}

