#ifndef TCPTUNNEL_H
#define TCPTUNNEL_H
#include "IPHeader.h"
#include "TCPHeader.h"
#include "TcpProxy.h"
#include "Config.h"

class TcpTunnel
{
public:
	TcpTunnel(){
	}
	
	TcpTunnel(int id, Socket* socket){
		this->id = id;
		this->clientSocket = socket;
		closed = false;
	}
	
	~TcpTunnel(){
		close();
	}
	
	void close(){
		closed = true;
		clearAllProxy();
	}
	
	bool isClose(){
		return closed;
	} 
	
	/*
	 * 接收数据包 建立新代理
	 */
	void processPacket(char* packet, int size){
		// 清除已关闭代理，防止数据发送给已关闭代理 
		clearCloseProxy();
		// 清除长时间未活动代理，减小内存使用 
		if (tcpProxys.size() > Config::CLIENT_MAX_TCPPROXY) {
			int clearNum =  clearExpireProxy();
			if(clearNum > 0) printf("[TcpTunnel]client(%ld) proxy number max, cleaned up number %d, now proxy number %ld.\n", id, clearNum, tcpProxys.size());
		}
		
		TcpProxy* tcpProxy = NULL;
		// 检查该代理是否创建 
		for (int i = 0; i < tcpProxys.size(); i++) {
			if (tcpProxys[i]->equal(packet)) {
				tcpProxy = tcpProxys[i];
				break;
			}
		}
		
		// 代理没创建，建立新代理 
		if(tcpProxy == NULL) {
			tcpProxy = new TcpProxy(clientSocket, packet);
			tcpProxy->processPacket(&tcpProxys, packet, true);
		}else {
			tcpProxy->processPacket(&tcpProxys, packet, false);
		}
	}
	
	/*
	 * 清除不活动代理
	 */
	int clearExpireProxy() {
		int ret = 0;
		for (int i = 0; i < tcpProxys.size(); i++) {
			TcpProxy* tcpProxy = tcpProxys[i];
			if(tcpProxy->isExpire()){
				tcpProxys.erase(tcpProxys.begin() + i);
				i--;
				ret++;
				delete tcpProxy;
			}
		}
		return ret;
	}
	
	/*
	 * 清除已关闭代理
	 */
	int clearCloseProxy() {
		int ret = 0;
		for (int i = 0; i < tcpProxys.size(); i++) {
			TcpProxy* tcpProxy = tcpProxys[i];
			if(tcpProxy->finishClose()){
				tcpProxys.erase(tcpProxys.begin() + i);
				i--;
				ret++;
				delete tcpProxy;
			}
		}
		return ret;
	}
	
	/*
	 * 清除所有代理
	 */
	int clearAllProxy() {
		int ret = tcpProxys.size();
		for (int i = 0; i < tcpProxys.size(); i++) {
			TcpProxy* tcpProxy = tcpProxys[0];
			tcpProxys.erase(tcpProxys.begin());
			i--;
			delete tcpProxy;
		}
		return ret;
	}

private:
	// id  
	long id;
	// 已关闭标志 
	bool closed;
	// 客户端套接字 
	Socket* clientSocket;
	// 代理连接容器  
	std::vector<TcpProxy*> tcpProxys;
};

#endif
