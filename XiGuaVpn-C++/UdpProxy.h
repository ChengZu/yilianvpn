#ifndef UDPPROXY_H
#define UDPPROXY_H
#include <ctime>
#include "Loop.h"
#include "Task.h"
#include "IPHeader.h"
#include "UDPHeader.h"
#include "Socket.h"
#include "DatagramSocket.h"
#include "Config.h"

class UdpProxy: public Task
{
public:
	static const int HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + UDPHeader::UDP_HEADER_SIZE;
	
	UdpProxy(){
	}
	
	UdpProxy(Socket* socket, char* packet){
		this->clientSocket = socket;
		IPHeader ipHeader = IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		UDPHeader udpHeader = UDPHeader(packet, ipHeaderLen);
		srcIp = ipHeader.getSourceIP();
		srcPort = udpHeader.getSourcePort();
		destIp = ipHeader.getDestinationIP();
		destPort = udpHeader.getDestinationPort();
		
		headerBytes = new char[UdpProxy::HEADER_SIZE];
		CommonMethods::arraycopy(packet, 0, headerBytes, 0, IPHeader::IP4_HEADER_SIZE);
		CommonMethods::arraycopy(packet, ipHeaderLen, headerBytes, IPHeader::IP4_HEADER_SIZE, UDPHeader::UDP_HEADER_SIZE);
		ipHeader = IPHeader(headerBytes, 0);
		udpHeader = UDPHeader(headerBytes, IPHeader::IP4_HEADER_SIZE);
		ipHeader.setSourceIP(destIp);
		udpHeader.setSourcePort(destPort);
		ipHeader.setDestinationIP(srcIp);
		udpHeader.setDestinationPort(srcPort);
		ipHeader.setHeaderLength(20);
		ipHeader.setProtocol(IPHeader::UDP);
		ipHeader.setFlagsAndOffset(0);
		closed = false;
		lastRefreshTime = std::time(NULL);
		
		this->destSocket = DatagramSocket(destIp, destPort);
		
		iloop = Loop(this, (char*)"udpProxy");
		iloop.join();
		
	}
	
	~UdpProxy(){
		closed = true;
		iloop.quit();
		destSocket.iClose();
		delete[] headerBytes;
	}
	
	void close(){
		closed = true;
		iloop.quit();
		destSocket.iClose();
		// printf("[UdpProxy]代理(%ld)已关闭。\n", id);
	}
	
	bool isClose(){
		return closed;
	}
	
	int sendToServer(char* bytes, int size, int offset = 0, int retry = 2){
		lastRefreshTime = std::time(NULL);
		
		int len = size - offset;
		char* data = new char[len];
		CommonMethods::arraycopy(bytes, offset, data, 0, len);
		int res = destSocket.socketSend(data, len);
		delete[] data;
		// 如果发送异常，放弃这个UDP连接 
		if(res == 0){
			close();
		}else if(res == -1) {
			perror("[UdpProxy]套接字错误消息");
			printf("[UdpProxy]代理(%ld%s)转发数据给服务器失败，retry=%d。\n", id, toString().c_str(), retry);
			if(retry > 0){
				sendToServer(bytes, size, offset, retry - 1);
			}
		}else if((res > 0) && (res < size)){
			sendToServer(bytes, size, (offset + res));
		}
		int dataSize = 0;
		if(res > 0) dataSize = res;
		if(retry == 0 && (offset + dataSize) != size){
			close();
			perror("[UdpProxy]套接字错误消息");
			printf("[UdpProxy]代理(%ld%s)转发数据给服务器失败，共%d字节，成功发送%d字节。\n", id, toString().c_str(), size, (offset + dataSize));
		}
		return res;
	}
	
	int sendToClient(char* bytes, int size, int offset = 0, int retry = 2){
		lastRefreshTime = std::time(NULL);
		
		int len = size - offset;
		char* data = new char[len];
		CommonMethods::arraycopy(bytes, offset, data, 0, len);
		int res = clientSocket->socketSend(data, len);
		delete[] data;
		// 如果发送异常，放弃这个UDP连接 
		if(res == 0){
			close();
		}else if(res == -1) {
			perror("[UdpProxy]套接字错误消息");
			printf("[UdpProxy]代理(%ld%s)转发数据给客户端失败，retry=%d。\n", id, toString().c_str(), retry);
			if(retry > 0){
				sendToClient(bytes, size, offset, retry - 1);
			}
		}else if((res > 0) && (res < size)){
			sendToClient(bytes, size, (offset + res));
		}
		int dataSize = 0;
		if(res > 0) dataSize = res;
		if(retry == 0 && (offset + dataSize) != size){
			close();
			perror("[UdpProxy]套接字错误消息");
			printf("[UdpProxy]代理(%ld%s)转发数据给客户端失败，共%d字节，成功发送%d字节。\n", id, toString().c_str(), size, (offset + dataSize));
		}
		return res;
	}
	
	void processPacket(std::vector<UdpProxy*> *udpProxys, char* packet, int size, bool newUdpProxy) {
		if(newUdpProxy) {
			udpProxys->push_back(this);
		}
		IPHeader ipHeader = IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		UDPHeader udpHeader = UDPHeader(packet, ipHeaderLen);
		int headerLen = ipHeaderLen + UDPHeader::UDP_HEADER_SIZE;
		int dataSize = ipHeader.getTotalLength() - headerLen;
		if(dataSize > 0){
			char* data = new char[dataSize];
			CommonMethods::arraycopy(packet, headerLen, data, 0, dataSize);
			sendToServer(data, dataSize);
			delete[] data;
		}
	}
	
	bool loop(){            
		bool ret = false;
		char* bytes = new char[Config::MUTE - UdpProxy::HEADER_SIZE];
		int size = destSocket.socketRecv(bytes, Config::MUTE - UdpProxy::HEADER_SIZE);
		if (size > 0) {
			char* packet = new char[UdpProxy::HEADER_SIZE + size];
			CommonMethods::arraycopy(headerBytes, 0, packet, 0, UdpProxy::HEADER_SIZE);
			CommonMethods::arraycopy(bytes, 0, packet, UdpProxy::HEADER_SIZE, size);
			updateUDPBuffer(packet, size);
			sendToClient(packet, UdpProxy::HEADER_SIZE + size);
			delete[] packet;
		}else if(size == 0){
			close();
			ret = true;
		}
		delete[] bytes;
		return ret;
	}

	void updateUDPBuffer(char* packet, int size){
		IPHeader ipHeader = IPHeader(packet, 0);
		UDPHeader udpHeader = UDPHeader(packet, IPHeader::IP4_HEADER_SIZE);
		ipHeader.setTotalLength(UdpProxy::HEADER_SIZE + size);
		udpHeader.setTotalLength(UDPHeader::UDP_HEADER_SIZE + size);
		udpHeader.ComputeUDPChecksum(ipHeader);
	}
	
	std::string toString(){
		std::stringstream ss;
    	ss <<"["<< CommonMethods::ipIntToString(destIp) << ":" << destPort << "]->[" << CommonMethods::ipIntToString(srcIp) << ":" << srcPort <<"]";
    	return ss.str();
	}
	
	bool isExpire(){
		long now = std::time(NULL);
		return (now - lastRefreshTime) > Config::UDPPROXY_EXPIRE_TIME;
	}
	
	bool equal(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		UDPHeader udpHeader = UDPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == udpHeader.getSourcePort()
				&& destIp == ipHeader.getDestinationIP()
				&& destPort == udpHeader.getDestinationPort();
	}
	
private:
	// 客户端套接字 
	Socket* clientSocket;
	// 连接目标服务器的UDP套接字 
	DatagramSocket destSocket;
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
	// 最后刷新时间 
	long lastRefreshTime;
};

#endif
