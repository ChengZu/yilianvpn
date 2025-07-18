#include <iostream>
#include "VpnServer.h"
#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
#endif // _WIN32

int main(int argc, char **argv){
	if(argc > 1){
		Config::PORT = atoi(argv[1]);
	}

	VpnServer vpnServer = VpnServer();
	
	int loop = 0;
	while(Loop::taskNum()){
		Loop::executeTasks();
		loop++;
		if(loop > 10000){
			loop = 0; 
			#ifdef _WIN32
        	Sleep(1);
    		#else
        	usleep(1);
    		#endif // _WIN32
    	}
	}
	printf("Vpn exit.\n");
	return 0;
}
