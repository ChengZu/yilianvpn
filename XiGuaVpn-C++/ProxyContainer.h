#ifndef PROXYCONTAINER_H
#define PROXYCONTAINER_H
#include "Config.h"
#include "Proxy.h"
#include "TcpProxy.h"
#include "UdpProxy.h"

class ProxyContainer 
{
public:
	ProxyContainer(){
	}
	
	ProxyContainer(int id, Socket* socket){
		this->clientId = id;
		this->clientSocket = socket;
		closed = false;
	}
	
	~ProxyContainer(){
		close();
	}
	
	void close(){
		closed = true;
		clearAllProxy();
	}
	
	bool isClosed(){
		return closed;
	} 
	
	/*
	 * 接收数据包 建立新代理
	 */
	void processPacket(char* packet, int size, char protocol){
		// 清除已关闭代理，防止数据发送给已关闭代理 
		clearClosedProxy();
		// 清除长时间未活动代理，减小内存使用 
		if (proxys.size() > Config::CLIENT_MAX_PROXY) {
			int clearNum =  clearExpireProxy();
			if(clearNum > 0) printf("[Proxycontainer]client(%ld) proxy number max, cleaned up number %d, now proxy number %ld.\n", clientId, clearNum, proxys.size());
		}
		
		Proxy* proxy = NULL;
		// 检查该代理是否创建 
		for (int i = 0; i < proxys.size(); i++) {
			if (proxys[i]->equal(packet)) {
				proxy = proxys[i];
				break;
			}
		}
		
		// 代理没创建，建立新代理 
		if(proxy == NULL) {
			if(protocol == IPHeader::TCP){
				proxy = new TcpProxy(clientId, clientSocket, packet, protocol);
			}else if(protocol == IPHeader::UDP){
				proxy = new UdpProxy(clientId, clientSocket, packet, protocol);
			}else{
				printf("[Proxycontainer]client(%ld) create new proxy(%s) error.\n", clientId, proxy->toString().c_str());
				return;
			}
			proxys.push_back(proxy);
			proxy->processFisrtPacket(packet, size);
		}else {
			proxy->processPacket(packet, size);
		}
	}
	
	/*
	 * 清除不活动代理
	 */
	int clearExpireProxy() {
		int ret = 0;
		for (int i = 0; i < proxys.size(); i++) {
			Proxy* proxy = proxys[i];
			if(proxy->isExpire()){
				proxys.erase(proxys.begin() + i);
				i--;
				ret++;
				delete proxy;
			}
		}
		return ret;
	}
	
	/*
	 * 清除已关闭代理
	 */
	int clearClosedProxy() {
		int ret = 0;
		for (int i = 0; i < proxys.size(); i++) {
			Proxy* proxy = proxys[i];
			if(proxy->isClosed()){
				proxys.erase(proxys.begin() + i);
				i--;
				ret++;
				delete proxy;
			}
		}
		return ret;
	}
	
	/*
	 * 清除所有代理
	 */
	int clearAllProxy() {
		int ret = proxys.size();
		for (int i = 0; i < proxys.size(); i++) {
			Proxy* proxy = proxys[0];
			proxys.erase(proxys.begin());
			i--;
			delete proxy;
		}
		return ret;
	}

private:
	// id  
	long clientId;
	// 已关闭标志 
	bool closed;
	// 客户端套接字 
	Socket* clientSocket;
	// 代理连接容器  
	std::vector<Proxy*> proxys;
};

#endif
