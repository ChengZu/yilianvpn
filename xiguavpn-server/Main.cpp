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
	if(argc > 2)
	{
		Config::USER_NAME = atoi(argv[2]);
	}
	if(argc > 3)
	{
		Config::USER_PASSWD = atoi(argv[3]);
	}

	VpnServer vpnServer;

	int frequency = 1000 * 1000;
	int loopCount = 0;
	// Æô¶¯ÈÎÎñ
	while(task_loop() > 0)
	{
		if(loopCount > frequency)
		{
			loopCount = 0;
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1);
#endif // _WIN32
		}
		loopCount++;
	}

	printf("[main]Vpn exit.\n");

	return 0;
}

