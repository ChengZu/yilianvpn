#ifndef TCPPROXY_H
#define TCPPROXY_H
#include <ctime>
#include "CommonMethods.h"
#include "Config.h"
#include "Loop.h"
#include "Task.h"
#include "IPHeader.h"
#include "TCPHeader.h"
#include "Proxy.h"
#include "Socket.h"

class CachePacket{
public:
	char* data;
	int size;
	CachePacket(){
	}
	CachePacket(char* data, int size){
		this->data = data;
		this->size = size;
	}
	~CachePacket(){
	}
};

class TcpProxy: public Proxy
{
public:
	TcpProxy(){
	}
	
	TcpProxy(long clientId, Socket* clientSocket, char* packet) : Proxy(clientId, clientSocket, packet){
		headerBytes = new char[Proxy::TCP_HEADER_SIZE];
		IPHeader ipHeader = IPHeader(headerBytes, 0);
		TCPHeader tcpHeader = TCPHeader(headerBytes, IPHeader::IP4_HEADER_SIZE);

		ipHeader.setHeaderLength(IPHeader::IP4_HEADER_SIZE);
		ipHeader.setTos(0x8); // 最大吞吐量 
		ipHeader.setIdentification(0);
		ipHeader.setFlagsAndOffset(0x4000); // 不要分片
		ipHeader.setTTL(32);
		ipHeader.setProtocol(IPHeader::TCP);
		ipHeader.setSourceIP(destIp);
		ipHeader.setDestinationIP(srcIp);

		tcpHeader.setDestinationPort(srcPort);
		tcpHeader.setSourcePort(destPort);
		tcpHeader.setHeaderLength(TCPHeader::TCP_HEADER_SIZE);
		tcpHeader.setFlag(0);
		tcpHeader.setWindow(65535);
		tcpHeader.setUrp(0);

		state = 0;
		connected = false;
	}
	
	~TcpProxy(){
		destSocket.iClose();
		delete[] headerBytes;
		for (int i = 0; i < cachePackets.size(); i++) {
			delete[] cachePackets[i].data;
		}
	}
	
	void RSTClient(std::string msg = "disconnect client by server") {
		close(msg);
		updateTCPBuffer(headerBytes, TCPHeader::RST, mySeq, myAck, 0);
		sendToClient(headerBytes, Proxy::TCP_HEADER_SIZE);
	}
	
	void close(std::string msg = "close done"){
		errorMsg = msg;
		closed = true;
		state = CLOSED;
		iloop.quit();
		destSocket.iClose();
		// printf("[TcpProxy]proxy(%s) closed, the msg is %s.\n", toString().c_str(), errorMsg.c_str());
	}
	
	bool isClosed(){
		return state == CLOSED;
	}
	
	int sendToServer(char* bytes, int size){
		lastClientRefreshTime = std::time(NULL);
		int res = -1;
		if(connected){
			res = sendData(&destSocket, bytes, size);
			if(res < size){
				perror("[Proxy]socket error msg");
				printf("[Proxy]proxy(%s) send data to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
			}
		}else{
			char* data = new char[size];
			CommonMethods::arraycopy(bytes, 0, data, 0 ,size);
			CachePacket packet = CachePacket(data, size);
			cachePackets.push_back(packet);
		}
		return res;
	}
	
	int sendToClient(char* bytes, int size){
		lastServerRefreshTime = std::time(NULL);
		int res = sendData(clientSocket, bytes, size);
		if(res < size){
			perror("[TcpProxy]socket error msg");
			printf("[TcpProxy]proxy(%s) send data to client fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
		}
		return res;
	}
	
	void processFisrtPacket(char* packet, int size) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		mySeq = 0;
		myAck = tcpHeader.getSeqID() + 1;
		int flags = tcpHeader.getFlag();
		
		if((flags & TCPHeader::SYN) == TCPHeader::SYN) {
			destSocket = Socket(destIp, destPort, Socket::TCP);
			processPacket(packet, size);
		}else{
			// printf("[TcpProxy]proxy(%s) recvive client first packet(%s) is not syn, flags is %d.\n", toString().c_str(), tcpHeader.toString().c_str(), flags);
			if((flags & TCPHeader::RST) == TCPHeader::RST){
				close("close by rst code");
			}else {
				RSTClient("close by fist code is syn");
			}
		}
	}
	
	
	void processPacket(char* packet, int size) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		int flags = tcpHeader.getFlag();
		
		if((flags & TCPHeader::SYN) == TCPHeader::SYN){
			processSYNPacket(packet);
		}else if((flags & TCPHeader::SYN) == TCPHeader::SYN && (flags & TCPHeader::ACK) == TCPHeader::ACK){
			processACKPacket(packet);
		}else if((flags & TCPHeader::FIN) == TCPHeader::FIN && (flags & TCPHeader::ACK) == TCPHeader::ACK){
			processFINPacket(packet);
		}else if((flags & TCPHeader::ACK) == TCPHeader::ACK){
			processACKPacket(packet);
		}else if((flags & TCPHeader::RST) == TCPHeader::RST){
			processRSTPacket(packet);
		}else if((flags & TCPHeader::PSH) == TCPHeader::PSH){
			processPSHPacket(packet);
		}else if((flags & TCPHeader::URG) == TCPHeader::URG){
			processURGPacket(packet);
		}else{
			printf("[TcpProxy]proxy(%s), packet flags %d program unable to process.\n", toString().c_str(), flags);
		}
	}
	
	void processSYNPacket(char* packet){
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		mySeq = 0;
		myAck = tcpHeader.getSeqID() + 1;
		updateTCPBuffer(headerBytes, (TCPHeader::SYN | TCPHeader::ACK), mySeq, myAck, 0);
		sendToClient(headerBytes, Proxy::TCP_HEADER_SIZE);
		state = SYN_RCVD;
		mySeq += 1; //recv packet ack = mySeq + 1, son mySeq += 1. 
	}
	
	void processACKPacket(char* packet) {
		switch (state) {
		case SYN_RCVD:
			processSYNRCVDACKPacket(packet);
			break;
		case CLOSE_WAIT:
			processCLOSEWAITACKPacket(packet);
			break;
		case LAST_ACK:
			processLASTACKPacket(packet);
			break;
		case CLOSED:
			printf("[TcpProxy]proxy(%s) process ack packet but state closed, state=%d.\n", toString().c_str(), state);
			break;
		case ESTABLISHED:
			processESTABLISHEDACKPacket(packet);
			break;
		default:
			printf("[TcpProxy]proxy(%s) process ack packet but state abnormal, state=%d.\n", toString().c_str(), state);
			break;
		}
	}
	
	void processSYNRCVDACKPacket(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		if (tcpHeader.getSeqID() == myAck && tcpHeader.getAckID() == mySeq) {
			state = ESTABLISHED;
		}else {
			printf("[TcpProxy]proxy(%s) initialization synchronous queue number fail.\n", toString().c_str());
		}
	}
	
	void processESTABLISHEDACKPacket(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		int headerLength = ipHeader.getHeaderLength() + tcpHeader.getHeaderLength();
		int dataSize = ipHeader.getTotalLength() - headerLength;
		int seq = tcpHeader.getSeqID();	
		if (seq == myAck) { 
			//printf("[TcpProxy]proxy(%s) recvive client ack queue number match.\n", toString().c_str()); 
			if(dataSize > 0){
				char* data = new char[dataSize];
				CommonMethods::arraycopy(packet, headerLength, data, 0, dataSize);
				sendToServer(data, dataSize);
				delete[] data;
				// 下一个序列号 
				myAck += dataSize;
			}else{
				// printf("[TcpProxy]proxy(%s) ESTABLISHED recvive client ack packet, but data length=%d.\n", toString().c_str(), dataSize); 
			}
		} else if (seq < myAck) {	
			int nextSeq = seq + dataSize;
			if (nextSeq > myAck) {
				printf("[TcpProxy]proxy(%s)more packets, recvive client ack packet abnormal, seq number %u max %u.\n", toString().c_str(), nextSeq, myAck);
			} else if(nextSeq < myAck){
				printf("[TcpProxy]proxy(%s)repeat packets, recvive client ack packet abnormal, seq number %u min %u.\n", toString().c_str(), nextSeq, myAck);
			}
		} else {
			printf("[TcpProxy]proxy(%s)miss packets，recvive client ack packet abnormal, seq number %u max %u.\n", toString().c_str(), seq, myAck);
			myAck = tcpHeader.getSeqID() + 1;
		}
		updateTCPBuffer(headerBytes, TCPHeader::ACK, mySeq, myAck, 0);
		// 发送数据收到ACK包 
		sendToClient(headerBytes, Proxy::TCP_HEADER_SIZE);
	}
	
	void processRSTPacket(char* packet) {
		// printf("[TcpProxy]proxy(%s) recvive client rst packet, closeing.\n", toString().c_str()); 
		close("close because recvive client rst packet");
	}
	
	void processURGPacket(char* packet) {

	}

	void processPSHPacket(char* packet) {

	}

	void processFINPacket(char* packet) {
		// printf("[TcpProxy]proxy(%s) start closeing by client, state=%d.\n", toString().c_str(), state); 
		updateTCPBuffer(headerBytes, TCPHeader::ACK, mySeq, myAck, 0);
		sendToClient(headerBytes, Proxy::TCP_HEADER_SIZE);
		state = CLOSE_WAIT;	
		processCLOSEWAITACKPacket(packet); 
	}
	
	void processCLOSEWAITACKPacket(char* packet) {
		updateTCPBuffer(headerBytes, (TCPHeader::FIN | TCPHeader::ACK), mySeq, myAck, 0);
		sendToClient(headerBytes, Proxy::TCP_HEADER_SIZE);
		state = LAST_ACK;
	}

	void processLASTACKPacket(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		int ack = tcpHeader.getAckID() - 1;
		int seq = tcpHeader.getSeqID() - 1;
		if (ack == mySeq && seq == myAck) {
			// printf("[TcpProxy]proxy(%s) LAST_ACK confirm success, seq %u:%u, ack %u:%u, close success.\n", toString().c_str(), seq, myAck, ack, mySeq);
			// 关闭完成, 释放资源 
			close();
		} else {
			printf("[TcpProxy]proxy(%s) LAST_ACK confirm fail, queue number mismatched, seq %u:%u, ack %u:%u, close fail.\n", toString().c_str(), seq, myAck, ack, mySeq);
		}
	}
	
	bool loop(){
		//printf("[TcpProxy]proxy(%s) loop.\n", toString().c_str());
		// 未检测到客户端断开，超时没数据也断开 
		if(this->isExpire()) return true;
		bool ret = false;
		// 与服务器未建立连接 
		if(!connected){
			if(cachePackets.size() > 0){
				int res = destSocket.socketSend(cachePackets[0].data, cachePackets[0].size);
				if(res == 0){
					close("server disconnected");
					return true;
				}else if(res == cachePackets[0].size){
					connected = true;
					delete[] cachePackets[0].data;
					cachePackets.erase(cachePackets.begin());
					for (int i = 0; i < cachePackets.size(); i++) {
						sendToServer(cachePackets[i].data, cachePackets[i].size);
						delete[] cachePackets[i].data;
						cachePackets.erase(cachePackets.begin());
						i--;
					}
				}else if(res > 0) {
					printf("[TcpProxy]proxy(%s) first packet send to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), cachePackets[0].size, res);
					close("init time send data to server error");
					return true;
				}
			}
			if(cachePackets.size() > 0 && (std::time(NULL) - createTime) > Config::TCP_CONNECT_TIMEOUT){
				printf("[TcpProxy]proxy(%s) connection initialization timeout with server, socket errorno %d.\n", toString().c_str(), errno);
				close("server connection initialization timeout");
				return true;
			}
			return false;
		}

		// 从服务器接收数据 
		char* bytes = new char[Config::MUTE - Proxy::TCP_HEADER_SIZE];
		int size = destSocket.socketRecv(bytes, Config::MUTE - Proxy::TCP_HEADER_SIZE);
		if (size > 0) {
			int packetLen = Proxy::TCP_HEADER_SIZE + size;
			char* packet = new char[packetLen];
			CommonMethods::arraycopy(headerBytes, 0, packet, 0, Proxy::TCP_HEADER_SIZE);
			CommonMethods::arraycopy(bytes, 0, packet, Proxy::TCP_HEADER_SIZE, size);
			updateTCPBuffer(packet, TCPHeader::ACK, mySeq, myAck, size);
			// 转发给客户端 
			sendToClient(packet, packetLen);
			mySeq += size;
			delete[] packet;
		}else if(size == 0){
			close("server disconnected");
			ret = true;
		}	
		delete[] bytes;
		return ret;
	}
	
	bool equal(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == tcpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP()	&& destPort == tcpHeader.getDestinationPort();
	}
	
	void updateTCPBuffer(char* packet, char flag, int seq, int ack, int dataSize){
		identification++;
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		ipHeader.setTotalLength(Proxy::TCP_HEADER_SIZE + dataSize);
		ipHeader.setIdentification(identification);
		tcpHeader.setFlag(flag);
		tcpHeader.setSeqID(seq);
		tcpHeader.setAckID(ack);
		tcpHeader.ComputeTCPChecksum(ipHeader);
	}

private:
	// 发送给客户端数据队列号 
	unsigned int mySeq;
	// 收到处理完成客户端数据队列号 
	unsigned int myAck;
	// packet标识符  
	int identification;
	
	// IP/TCP通信状态 
	int state; 
	// 与服务器建立连接
	bool connected; 
	// 未与服务器建立连接发来的数据 
	std::vector<CachePacket> cachePackets;
	static const int SYN_RCVD = 1;
	static const int ESTABLISHED = 2;
	static const int CLOSE_WAIT = 3;
	static const int LAST_ACK = 4;
	static const int CLOSED = 5;
};

#endif

