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
	
	UdpProxy(long clientId, Socket* socket, char* packet){
		this->clientId = clientId;
		this->clientSocket = socket;
		IPHeader ipHeader = IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		UDPHeader udpHeader = UDPHeader(packet, ipHeaderLen);
		srcIp = ipHeader.getSourceIP();
		srcPort = udpHeader.getSourcePort();
		destIp = ipHeader.getDestinationIP();
		destPort = udpHeader.getDestinationPort();
		
		headerBytes = new char[UdpProxy::HEADER_SIZE];
		//CommonMethods::arraycopy(packet, 0, headerBytes, 0, IPHeader::IP4_HEADER_SIZE);
		//CommonMethods::arraycopy(packet, ipHeaderLen, headerBytes, IPHeader::IP4_HEADER_SIZE, UDPHeader::UDP_HEADER_SIZE);
		ipHeader = IPHeader(headerBytes, 0);
		udpHeader = UDPHeader(headerBytes, IPHeader::IP4_HEADER_SIZE);

		ipHeader.setHeaderLength(20);
		ipHeader.setTos(0x8); // 最大吞吐量 
		ipHeader.setIdentification(0);
		ipHeader.setFlagsAndOffset(0x4000); // 不要分片
		ipHeader.setTTL(32);
		ipHeader.setProtocol(IPHeader::UDP);
		ipHeader.setSourceIP(destIp);
		ipHeader.setDestinationIP(srcIp);
		
		udpHeader.setSourcePort(destPort);
		udpHeader.setDestinationPort(srcPort);
		
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

	void disConnectClient(std::string msg) {
		printf("[TcpProxy]proxy(%s) start closeing by server.\n", toString().c_str()); 
		errorMsg = msg;
		close();
	}
	
	void close(std::string msg = ""){
		errorMsg = msg;
		closed = true;
		iloop.quit();
		destSocket.iClose();
		printf("[UdpProxy]proxy(%s) closed, the msg is %s.\n", toString().c_str(), errorMsg.c_str());
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
			disConnectClient("server disconnected");
		}else if(res == -1) {
			// perror("[UdpProxy]socket error msg");
			// printf("[UdpProxy]proxy(%s) forward data to server fail, retry=%d.\n", toString().c_str(), retry);
			if(retry > 0){
				sendToServer(bytes, size, offset, retry - 1);
			}
		}else if((res > 0) && (res < size)){
			sendToServer(bytes, size, (offset + res));
		}
		int dataSize = 0;
		if(res > 0) dataSize = res;
		if(retry == 0 && (offset + dataSize) != size){
			disConnectClient("send data to server error");
			perror("[UdpProxy]socket error msg");
			printf("[UdpProxy]proxy(%s) forward data to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, (offset + dataSize));
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
			disConnectClient("client disconnected");
		}else if(res == -1) {
			// perror("[UdpProxy]socket error msg");
			// printf("[UdpProxy]proxy(%s) forward data to client fail, retry=%d.\n", toString().c_str(), retry);
			if(retry > 0){
				sendToClient(bytes, size, offset, retry - 1);
			}
		}else if((res > 0) && (res < size)){
			sendToClient(bytes, size, (offset + res));
		}
		int dataSize = 0;
		if(res > 0) dataSize = res;
		if(retry == 0 && (offset + dataSize) != size){
			disConnectClient("send data to client error");
			perror("[UdpProxy]socket error msg");
			printf("[UdpProxy]proxy(%s) forward data to client fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, (offset + dataSize));
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
			// 转发给服务器  
			sendToServer(data, dataSize);
			delete[] data;
		}
	}
	
	bool loop(){            
		bool ret = false;
		// 从服务器接收数据  
		char* bytes = new char[Config::MUTE - UdpProxy::HEADER_SIZE];
		int size = destSocket.socketRecv(bytes, Config::MUTE - UdpProxy::HEADER_SIZE);
		if (size > 0) {
			char* packet = new char[UdpProxy::HEADER_SIZE + size];
			CommonMethods::arraycopy(headerBytes, 0, packet, 0, UdpProxy::HEADER_SIZE);
			CommonMethods::arraycopy(bytes, 0, packet, UdpProxy::HEADER_SIZE, size);
			updateUDPBuffer(packet, size);
			// 转发给客户端 
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
    	ss << "[" << clientId << ":" << id <<"]["<< CommonMethods::ipIntToString(srcIp) << ":" << srcPort << "]->[" << CommonMethods::ipIntToString(destIp) << ":" << destPort <<"]";
    	return ss.str();
	}
	
	bool isExpire(){
		long now = std::time(NULL);
		return (now - lastRefreshTime) > Config::UDPPROXY_EXPIRE_TIME;
	}
	
	bool equal(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		UDPHeader udpHeader = UDPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == udpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP()	&& destPort == udpHeader.getDestinationPort();
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
	// 错误消息 
	std::string  errorMsg;
	// 客户端id 
	long clientId;
};

#endif
