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
#include "RingQueueBuffer.h"

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
		muteBuffer = new char[Config::MUTE];
		
		iloop = Loop(this, (char*)"proxy");
		iloop.join();
	}
	
	~Proxy(){
		closed = true;
		delete[] muteBuffer;
	} 
	
	virtual void close(std::string msg = ""){}
	virtual bool isClosed(){return false;}
	virtual void processFisrtPacket(char* packet, int size){}
	virtual void processPacket(char* packet, int size){}
	virtual bool equal(char* packet) {return false;}

	int sendData(Socket *socket, char* bytes, int size, int offset = 0, int retry = 2){
		int len = size - offset;
		int res = socket->socketSend(bytes + offset, len);
		// 如果发送异常，放弃这个SOcket连接 
		if(res == 0){
			close("server disconnected");
		}else if(res == -1) {
			if(retry > 0){
				res = sendData(socket, bytes, size, offset, retry - 1);
			}else{
				close("retry=0 with send data error");
				if(offset > 0)
					res = offset;
				if(res > 0)
					printf("[Proxy]proxy(%s) forward data fail, broke packet send, total %d send bytes=%d.\n", toString().c_str(), size, offset);
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
	
	int sendToBuffer(Socket* socket, RingQueueBuffer* buf, char* bytes, int size){
		// 先发送缓冲区 
		int res = 0;
		int rlen = buf->availableReadLength();
		if(rlen > 0){
			char* data = new char[rlen];
			buf->poll(data, rlen);
			res = sendData(socket, data, rlen);
			//res = socket->sendToBuffer(data, rlen);
			res = res > 0 ? res : 0;
			delete[] data;
			if(res < rlen) {
				buf->push(data + res, rlen - res);
				int notWriteSize = size - buf->push(bytes, size);
				if(notWriteSize > 0){
					printf("ERROR:[Proxy]proxy(%s) RingQueueBuffer no buffer space %d less %d.\n", toString().c_str(), buf->availableWriteLength(), notWriteSize);
				}
				return size - notWriteSize;
			}
		}
		
		res = sendData(socket, bytes, size);
		//res = socket->sendToBuffer(bytes, size);
		res = res > 0 ? res : 0;
		if(res < size){
			//缓存未发送的数据 
			int notWriteSize = size - res - buf->push(bytes + res, size - res);
			if(notWriteSize > 0){
				printf("ERROR:[Proxy]proxy(%s) RingQueueBuffer no buffer space %d less %d.\n", toString().c_str(), buf->availableWriteLength(), notWriteSize);
			}
			return size - notWriteSize;
		}
		return size;
	}
	
	bool isExpire(){
		long now = std::time(NULL);
		return (now - lastClientRefreshTime) > Config::PROXY_EXPIRE_TIME || (now - lastServerRefreshTime) > Config::PROXY_EXPIRE_TIME;
	}
	
	
	std::string toString(){
		std::stringstream ss;
    	ss << "[" << clientId << ":" << getId() << "][" << CommonMethods::ipIntToString(srcIp) << ":" << srcPort << "]->[" << CommonMethods::ipIntToString(destIp) << ":" << destPort <<"]";
    	return ss.str();
	}
protected:
	// 客户端套接字 
	Socket* clientSocket;
	// 连接目标服务器的UDP套接字 
	Socket destSocket;
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
	// 数据缓冲 
	char* muteBuffer;
	// 客户端数据缓冲 
	RingQueueBuffer clientRingQueueBuffer;
	// 我的数据缓冲  
	RingQueueBuffer myRingQueueBuffer;
};

#endif
