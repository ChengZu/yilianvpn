#ifndef PROXY_H
#define PROXY_H
#include <ctime>
#include "Config.h"
#include "Task.h"
#include "Loop.h"
#include "IPHeader.h"
#include "TCPHeader.h"
#include "UDPHeader.h"
#include "Socket.h"

class Proxy: public Task
{
public:
	static const int TCP_HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + TCPHeader::TCP_HEADER_SIZE;
	static const int UDP_HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + UDPHeader::UDP_HEADER_SIZE;
	
	Proxy(){
	}
	
	Proxy(long clientId, Socket* clientSocket, char* packet){
		this->clientId = clientId;
		this->clientSocket = clientSocket;
		IPHeader ipHeader = IPHeader(packet, 0);
		UDPHeader udpHeader = UDPHeader(packet, ipHeader.getHeaderLength());
		srcIp = ipHeader.getSourceIP();
		srcPort = udpHeader.getSourcePort();
		destIp = ipHeader.getDestinationIP();
		destPort = udpHeader.getDestinationPort();
		
		closed = false;
		createTime = std::time(NULL);
		lastClientRefreshTime = std::time(NULL);
		lastServerRefreshTime = std::time(NULL);
		
		iloop = Loop(this, (char*)"proxy");
		iloop.join();
	}
	
	~Proxy(){
		closed = true;
		iloop.quit();
	} 
	
	virtual void close(std::string msg = ""){}
	virtual bool isClosed(){
		return closed; 
	}
	virtual void processFisrtPacket(char* packet, int size){}
	virtual void processPacket(char* packet, int size){}

	int sendData(Socket *socket, char* bytes, int size, int offset = 0, int retry = 2){
		int len = size - offset;
		char* data = new char[len];
		CommonMethods::arraycopy(bytes, offset, data, 0, len);
		int res = socket->socketSend(data, len);
		delete[] data;
		// 如果发送异常，放弃这个SOcket连接 
		if(res == 0){
			close("server disconnected");
		}else if(res == -1) {
			if(retry > 0){
				res = sendData(socket, bytes, size, offset, retry - 1);
			}else{
				//printf("[Proxy]proxy(%s) forward data fail, errorno %d total %d send bytes=%d.\n", toString().c_str(), res, size, offset);
				close("retry=0 with send data error");
				res = offset;
			}
		}else if((res > 0) && (res < size)){
			res = sendData(socket, bytes, size, (offset + res));
		}else if(res == len){
			return offset + res;
		}else{
			res = -2;
		}
		return res;
	}
	
	bool isExpire(){
		long now = std::time(NULL);
		return (now - lastClientRefreshTime) > Config::PROXY_EXPIRE_TIME || (now - lastServerRefreshTime) > Config::PROXY_EXPIRE_TIME;
	}
	
	bool equal(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		UDPHeader udpHeader = UDPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == udpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP()	&& destPort == udpHeader.getDestinationPort();
	}
	
	bool operator==(const Proxy &proxy){
		return id ==proxy.id;
	}
	
	std::string toString(){
		std::stringstream ss;
    	ss << "[" << clientId << ":" << id << "][" << CommonMethods::ipIntToString(srcIp) << ":" << srcPort << "]->[" << CommonMethods::ipIntToString(destIp) << ":" << destPort <<"]";
    	return ss.str();
	}
protected:
	// 客户端套接字 
	Socket* clientSocket;
	// 连接目标服务器的UDP套接字 
	Socket destSocket;
	// 保存UDP连接信息, 头部信息 
	char* headerBytes;
	// 已关闭状态 
	bool closed;
	// 源ip 
	unsigned int srcIp;
	// 源端口 
	unsigned short srcPort;
	// 目标ip 
	unsigned int destIp;
	// 目标端口 
	unsigned short destPort; 
	// 接收目标服务器数据任务 
	Loop iloop;
	// 错误消息 
	std::string  errorMsg;
	// 客户端id 
	long clientId;
	// 创建时间
	long createTime; 
	// 客户端最后刷新时间 
	long lastClientRefreshTime;
	// 服务器最后刷新时间 
	long lastServerRefreshTime;
};

#endif
