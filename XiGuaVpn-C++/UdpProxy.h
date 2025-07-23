#ifndef UDPPROXY_H
#define UDPPROXY_H
#include <ctime>
#include "Config.h"
#include "Loop.h"
#include "Task.h"
#include "IPHeader.h"
#include "UDPHeader.h"
#include "Proxy.h"
#include "Socket.h"


class UdpProxy: public Proxy
{
public:	
	UdpProxy(){
	}
	
	UdpProxy(long clientId, Socket* clientSocket, char* packet) : Proxy(clientId, clientSocket, packet){
		headerBytes = new char[Proxy::UDP_HEADER_SIZE];
		IPHeader ipHeader = IPHeader(headerBytes, 0);
		UDPHeader udpHeader = UDPHeader(headerBytes, IPHeader::IP4_HEADER_SIZE);

		ipHeader.setHeaderLength(IPHeader::IP4_HEADER_SIZE);
		ipHeader.setTos(0x8); // 最大吞吐量 
		ipHeader.setIdentification(0);
		ipHeader.setFlagsAndOffset(0x4000); // 不要分片
		ipHeader.setTTL(32);
		ipHeader.setProtocol(IPHeader::UDP);
		ipHeader.setSourceIP(destIp);
		ipHeader.setDestinationIP(srcIp);
		
		udpHeader.setSourcePort(destPort);
		udpHeader.setDestinationPort(srcPort);
			
		destSocket = Socket(destIp, destPort, Socket::UDP);
	}
	
	~UdpProxy(){
		destSocket.iClose();
		delete[] headerBytes;
	}
	
	void close(std::string msg = ""){
		errorMsg = msg;
		closed = true;
		iloop.quit();
		destSocket.iClose();
		// printf("[UdpProxy]proxy(%s) closed, the msg is %s.\n", toString().c_str(), errorMsg.c_str());
	}
	
	bool isClosed(){
		return closed;
	}
	
	int sendToServer(char* bytes, int size){
		lastClientRefreshTime = std::time(NULL);
		int res = sendData(&destSocket, bytes, size);
		if(res < size){
			perror("[UdpProxy]socket error msg");
			printf("[UdpProxy]proxy(%s) send data to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
		}
		return res;
	}
	
	int sendToClient(char* bytes, int size){
		lastServerRefreshTime = std::time(NULL);
		int res = sendData(clientSocket, bytes, size);
		if(res < size){
			perror("[UdpProxy]socket error msg");
			printf("[UdpProxy]proxy(%s) send data to client fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
		}
		return res;
	}
	
	void processFisrtPacket(char* packet, int size) {
		processPacket(packet, size);
	}
	
	void processPacket(char* packet, int size) {
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
		//printf("[UdpProxy]proxy(%s) loop.\n", toString().c_str());
		// 未检测到客户端断开，超时没数据也断开 
		if(this->isExpire()) return true;
		bool ret = false;
		// 从服务器接收数据  
		char* bytes = new char[Config::MUTE - Proxy::UDP_HEADER_SIZE];
		int size = destSocket.socketRecv(bytes, Config::MUTE - Proxy::UDP_HEADER_SIZE);
		if (size > 0) {
			int packetLen = Proxy::UDP_HEADER_SIZE + size;
			char* packet = new char[packetLen];
			CommonMethods::arraycopy(headerBytes, 0, packet, 0, Proxy::UDP_HEADER_SIZE);
			CommonMethods::arraycopy(bytes, 0, packet, Proxy::UDP_HEADER_SIZE, size);
			updateUDPBuffer(packet, size);
			// 转发给客户端 
			sendToClient(packet, packetLen);
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
		UDPHeader udpHeader = UDPHeader(packet, ipHeader.getHeaderLength());
		ipHeader.setTotalLength(Proxy::UDP_HEADER_SIZE + size);
		udpHeader.setTotalLength(UDPHeader::UDP_HEADER_SIZE + size);
		udpHeader.ComputeUDPChecksum(ipHeader);
	}
};

#endif
