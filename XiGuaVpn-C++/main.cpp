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
	
	// 启动任务 
	int loopNum = 0;
	while(Loop::taskNum()){ // 任务数大于0 
		Loop::executeTasks();
		// CPU频率/(10000*tasknum)=秒的时间执行睡眠1ms, 降低cpu负荷 
		if(loopNum >= (10000 * Loop::taskNum())){
		#ifdef _WIN32
        	Sleep(1);
    	#else
        	usleep(1);
    	#endif // _WIN32
    		loopNum = 0; 
    	}
    	loopNum++;
	}
	
	printf("Vpn exit.\n");
	
	return 0;
}
