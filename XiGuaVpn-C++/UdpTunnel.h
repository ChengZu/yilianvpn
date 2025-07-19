#ifndef UDPTUNNEL_H
#define UDPTUNNEL_H
#include "UDPHeader.h"
#include "UdpProxy.h"
#include "Config.h"

class UdpTunnel
{
public:
	
	UdpTunnel(){
	}
	
	UdpTunnel(long id, Socket* socket){
		this->id = id;
		this->clientSocket = socket;
		closed = false;
	}
	
	~UdpTunnel(){
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
	void processPacket(char* packet, int size) {
		// 清除已关闭代理，防止数据发送给已关闭代理  
		clearCloseProxy();
		// 清除长时间未活动代理，减小内存使用 
		if (udpProxys.size() > Config::CLIENT_MAX_UDPPROXY) {
			int clearNum = clearExpireProxy();
			if (clearNum > 0) printf("[UdpTunnel]客户端(%ld)代理数量达到最大，已清理长时间未活动代理数量：%d, 现在代理数量：%ld。\n", id, clearNum, udpProxys.size());
		}

		// 检查该代理是否已创建 
		UdpProxy* proxy = NULL;
		for (int i = 0; i < udpProxys.size(); i++) {
			if (udpProxys[i]->equal(packet)) {
				proxy = udpProxys[i];
				break;
			}
		}

		// 代理没创建，建立新代理 
		if (proxy == NULL) {
			proxy = new UdpProxy(clientSocket, packet);
			proxy->processPacket(&udpProxys, packet, size, true);
		}else {
			proxy->processPacket(&udpProxys, packet, size, false);
		}
	}
	
	/*
	 * 清除过期代理
	 */
	int clearExpireProxy() {
		int ret = 0;
		for (int i = 0; i < udpProxys.size(); i++) {
			UdpProxy* udpProxy = udpProxys[i];
			if(udpProxy->isExpire()){
				udpProxys.erase(udpProxys.begin() + i);
				i--;
				ret++;
				delete udpProxy;
			}
		}
		return ret;
	}

	/*
	 * 清除已关闭代理
	 */
	int clearCloseProxy() {
		int ret = 0;
		for (int i = 0; i < udpProxys.size(); i++) {
			UdpProxy* udpProxy = udpProxys[i];
			if(udpProxy->isClose()){
				udpProxys.erase(udpProxys.begin() + i);
				i--;
				ret++;
				delete udpProxy;
			}
		}
		return ret;
	}

	/*
	 * 清除所有代理
	 */
	int clearAllProxy() {
		int ret = udpProxys.size();
		for (int i = 0; i < udpProxys.size(); i++) {
			UdpProxy* udpProxy = udpProxys[0];
			udpProxys.erase(udpProxys.begin());
			i--;
			delete udpProxy;
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
	std::vector<UdpProxy*> udpProxys;
};

#endif
