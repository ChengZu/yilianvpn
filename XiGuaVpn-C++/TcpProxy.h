#ifndef TCPPROXY_H
#define TCPPROXY_H
#include <ctime>
#include "CommonMethods.h"
#include "Loop.h"
#include "Task.h"
#include "IPHeader.h"
#include "TCPHeader.h"
#include "Socket.h"
#include "Config.h"

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

class TcpProxy: public Task
{
public:
	static const int HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + TCPHeader::TCP_HEADER_SIZE;
	
	TcpProxy(){
	}
	
	TcpProxy(long clientId, Socket* socket, char* packet){
		this->clientId = clientId;
		this->clientSocket = socket;
		IPHeader ipHeader = IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		TCPHeader tcpHeader = TCPHeader(packet, ipHeaderLen);
		srcIp = ipHeader.getSourceIP();
		srcPort = tcpHeader.getSourcePort();
		destIp = ipHeader.getDestinationIP();
		destPort = tcpHeader.getDestinationPort();
		mySeq = 0;
		myAck = tcpHeader.getSeqID() + 1;
		
		headerBytes = new char[TcpProxy::HEADER_SIZE];
		ipHeader = IPHeader(headerBytes, 0);
		tcpHeader = TCPHeader(headerBytes, IPHeader::IP4_HEADER_SIZE);
		
		ipHeader.setHeaderLength(20);
		ipHeader.setTos(0x8); // 最大吞吐量 
		ipHeader.setIdentification(0);
		ipHeader.setFlagsAndOffset(0x4000); // 不要分片
		ipHeader.setTTL(32);
		ipHeader.setProtocol(IPHeader::TCP);
		ipHeader.setDestinationIP(srcIp);
		ipHeader.setSourceIP(destIp);
		
		tcpHeader.setDestinationPort(srcPort);
		tcpHeader.setSourcePort(destPort);
		tcpHeader.setHeaderLength(20);
		tcpHeader.setFlag(0);
		tcpHeader.setWindow(65535);
		tcpHeader.setUrp(0);

		
		state = 0;
		connected = false;
		createTime = std::time(NULL);
		lastRefreshTime = std::time(NULL);
		errorMsg =""; 
		
		this->destSocket = Socket(destIp, destPort);
		
		iloop = Loop(this, (char*)"TcpProxy");
		iloop.join();
	}
	
	~TcpProxy(){
		iloop.quit();
		destSocket.iClose();
		delete[] headerBytes;
		for (int i = 0; i < cachePackets.size(); i++) {
			delete[] cachePackets[i].data;
		}
	}
	
	void disConnectClient(std::string msg = "disconnect client by server") {
		// printf("[TcpProxy]proxy(%s) start closeing by server, state=%d.\n", toString().c_str(), state); 
		errorMsg = msg;
		close();           
	}
	
	void resetClient(std::string msg = "rest client by server") {
		errorMsg = msg;
		close();
		updateTCPBuffer(headerBytes, TCPHeader::RST, mySeq, myAck, 0);
		sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
	}
	
	void close(std::string msg = "close done"){
		errorMsg = msg;
		state = CLOSED;
		iloop.quit();
		destSocket.iClose();
		// printf("[TcpProxy]proxy(%s) closed, the msg is %s.\n", toString().c_str(), errorMsg.c_str());
	}
	
	bool isClose(){
		return state == CLOSED;
	}
	
	int sendToServer(char* bytes, int size, int offset = 0, int retry = 2){
		lastRefreshTime = std::time(NULL);
		int res = -1;
		if(connected){
			int len = size - offset;
			char* data = new char[len];
			CommonMethods::arraycopy(bytes, offset, data, 0, len);
			res = destSocket.socketSend(data, len);
			delete[] data;
			// 如果发送异常，放弃这个TCP连接 
			if(res == 0){
				disConnectClient("server disconnected");
			}else if(res == -1) {
				// perror("[TcpProxy]socket error msg");
				// printf("[TcpProxy]proxy(%s) forward data to server fail, retry=%d.\n", toString().c_str(), retry);
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
				perror("[TcpProxy]socket error msg");
				printf("[TcpProxy]proxy(%s) forward data to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, (offset + dataSize));
			}
		}else{
			char* data = new char[size];
			CommonMethods::arraycopy(bytes, 0, data, 0 ,size);
			CachePacket packet = CachePacket(data, size);
			cachePackets.push_back(packet);
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
		// 如果发送异常，放弃这个TCP连接 
		if(res == 0){
			disConnectClient("client disconnected");
		}else if(res == -1) {
			// perror("[TcpProxy]socket error msg");
			// printf("[TcpProxy]proxy(%s) forward data to client fail, retry=%d.\n", toString().c_str(), retry);
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
			perror("[TcpProxy]socket error msg");
			printf("[TcpProxy]proxy(%s) forward data to client fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, (offset + dataSize));
		}
		return res;
	}
	
	void processFisrtPacket(char* packet, int size) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, IPHeader::IP4_HEADER_SIZE);
		int flags = tcpHeader.getFlag();
		// 第一个TCP包不是SYN, 销毁该代理 
		if((flags & TCPHeader::SYN) != TCPHeader::SYN) {
			// printf("[TcpProxy]proxy(%s) recvive client first packet(%s) is not syn, flags is %d.\n", toString().c_str(), tcpHeader.toString().c_str(), flags);
			// 发送RST关闭客户端TCP连接(释放资源) 
			if((flags & TCPHeader::RST) != TCPHeader::RST){
				resetClient("recvive client first packet is not syn, send rst packet");
			}else{
				disConnectClient("recvive client first packet is not syn, closeing");
			}
			return;
		}
		processPacket(packet, size);
	}
	
	
	void processPacket(char* packet, int size) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, IPHeader::IP4_HEADER_SIZE);
		int flags = tcpHeader.getFlag();
		
		if((flags & TCPHeader::SYN) == TCPHeader::SYN){
			processSYNPacket(packet);
		}else
		
		if((flags & TCPHeader::SYN) == TCPHeader::SYN && (flags & TCPHeader::ACK) == TCPHeader::ACK){
			processACKPacket(packet);
		}else
		
		if((flags & TCPHeader::FIN) == TCPHeader::FIN && (flags & TCPHeader::ACK) == TCPHeader::ACK){
			processFINPacket(packet);
		}else
		
		if((flags & TCPHeader::ACK) == TCPHeader::ACK){
			processACKPacket(packet);
		}else
		
		if((flags & TCPHeader::RST) == TCPHeader::RST){
			processRSTPacket(packet);
		}else
		
		if((flags & TCPHeader::PSH) == TCPHeader::PSH){
			processPSHPacket(packet);
		}else
		
		if((flags & TCPHeader::URG) == TCPHeader::URG){
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
		sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
		mySeq += 1;
		state = SYN_RCVD;
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
			printf("[TcpProxy]proxy(%s) process ack packet state closed, state=%d.\n", toString().c_str(), state);
			break;
		case ESTABLISHED:
			processESTABLISHEDACKPacket(packet);
			break;
		default:
			printf("[TcpProxy]proxy(%s) process ack packet state abnormal, state=%d.\n", toString().c_str(), state);
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
		int dataSize = ipHeader.getTotalLength() - ipHeader.getHeaderLength() - tcpHeader.getHeaderLength();
		int sequenceNumber = tcpHeader.getSeqID();	
		if (sequenceNumber == myAck) { 
			//printf("[TcpProxy]proxy(%s) recvive client ack queue number match.\n", toString().c_str()); 
			if (dataSize > 0) {
				char* data = new char[dataSize];
				CommonMethods::arraycopy(packet, (ipHeader.getHeaderLength() + tcpHeader.getHeaderLength()), data, 0, dataSize);
				sendToServer(data, dataSize);
				delete[] data;
				// 下一个序列号 
				myAck += dataSize;
				updateTCPBuffer(headerBytes, TCPHeader::ACK, mySeq, myAck, 0);
				// 发送数据收到ACK包 
				sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
			}
		} else if (sequenceNumber < myAck) {	
			int nextSeq = sequenceNumber + dataSize;
			if (nextSeq > myAck) {
				printf("[TcpProxy]proxy(%s) recvive client ack packet abnormal, seq number %u max %u, more packets.\n", toString().c_str(), nextSeq, myAck);
			} else if(nextSeq < myAck){
				printf("[TcpProxy]proxy(%s) recvive client ack packet abnormal, seq number %u min %u, repeat packets.\n", toString().c_str(), nextSeq, myAck);
			}
		} else {
			printf("[TcpProxy]proxy(%s) recvive client ack packet abnormal, seq number %u max %u, miss packets.\n", toString().c_str(), sequenceNumber, myAck);
		}
	}
	
	void processRSTPacket(char* packet) {
		printf("[TcpProxy]proxy(%s) recvive client rst packet, closeing.\n", toString().c_str()); 
		close("close because recvive client rst packet");
	}
	
	void processURGPacket(char* packet) {

	}

	void processPSHPacket(char* packet) {

	}

	void processFINPacket(char* packet) {
		// printf("[TcpProxy]proxy(%s) start closeing by client, state=%d.\n", toString().c_str(), state); 
		iloop.quit();
		destSocket.iClose();
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		updateTCPBuffer(headerBytes, TCPHeader::ACK, mySeq, myAck, 0);
		sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
		state = CLOSE_WAIT;	
		processCLOSEWAITACKPacket(packet); 
	}
	
	void processCLOSEWAITACKPacket(char* packet) {
		updateTCPBuffer(headerBytes, (TCPHeader::FIN | TCPHeader::ACK), mySeq, myAck, 0);
		sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
		state = LAST_ACK;
	}

	void processLASTACKPacket(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		int seq = tcpHeader.getAckID() - 1;
		int ack = tcpHeader.getSeqID() - 1;
		if (seq == mySeq && ack == myAck) {
			// printf("[TcpProxy]proxy(%s) LAST_ACK confirm success, seq %u:%u, ack %u:%u, close suceess.\n", toString().c_str(), seq, mySeq, ack, myAck);
			// 关闭完成, 释放资源 
			close();
		} else {
			printf("[TcpProxy]proxy(%s) LAST_ACK confirm fail, queue number mismatched, seq %u:%u, ack %u:%u, close fail.\n", toString().c_str(), seq, mySeq, ack, myAck);
		}
	}
	
	bool loop(){
		bool ret = false;
		
		// 与服务器未建立连接 
		if(!connected){
			if(cachePackets.size() > 0){
				int res = destSocket.socketSend(cachePackets[0].data, cachePackets[0].size);
				if(res == 0){
					disConnectClient("server disconnected");
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
					disConnectClient("send data to server error");
					return true;
				}
			}
			if(cachePackets.size() > 0 && (std::time(NULL) - createTime) > Config::TCP_CONNECT_TIMEOUT){
				printf("[TcpProxy]proxy(%s) connection initialization timeout with server, socket errorno %d.\n", toString().c_str(), errno);
				disConnectClient("server connection initialization timeout");
				return true;
			}
			return false;
		}

		// 从服务器接收数据 
		char* bytes = new char[Config::MUTE - TcpProxy::HEADER_SIZE];
		int size = destSocket.socketRecv(bytes, Config::MUTE - TcpProxy::HEADER_SIZE);
		if (size > 0) {
			int packetLen = TcpProxy::HEADER_SIZE + size;
			char* packet = new char[packetLen];
			CommonMethods::arraycopy(headerBytes, 0, packet, 0, TcpProxy::HEADER_SIZE);
			CommonMethods::arraycopy(bytes, 0, packet, TcpProxy::HEADER_SIZE, size);
			updateTCPBuffer(packet, TCPHeader::ACK, mySeq, myAck, size);
			// 转发给客户端 
			sendToClient(packet, packetLen);
			mySeq += size;
			delete[] packet;
		}else if(size == 0){
			disConnectClient("server disconnected");
			ret = true;
		}	
		delete[] bytes;
		return ret;
	}
	
	void updateTCPBuffer(char* packet, char flag, int seq, int ack, int dataSize){
		identification++;
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, IPHeader::IP4_HEADER_SIZE);
		ipHeader.setTotalLength(TcpProxy::HEADER_SIZE + dataSize);
		ipHeader.setIdentification(identification);
		tcpHeader.setFlag(flag);
		tcpHeader.setSeqID(seq);
		tcpHeader.setAckID(ack);
		tcpHeader.ComputeTCPChecksum(ipHeader);
	}
	
	std::string toString(){
		std::stringstream ss;
    	ss << "[" << clientId << ":" << id <<"]["<< CommonMethods::ipIntToString(srcIp) << ":" << srcPort << "]->[" << CommonMethods::ipIntToString(destIp) << ":" << destPort <<"]";
    	return ss.str();
	}
	
	bool isExpire(){
		long now = std::time(NULL);
		return (now - lastRefreshTime) > Config::TCPPROXY_EXPIRE_TIME;
	}
	
	bool equal(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == tcpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP() && destPort == tcpHeader.getDestinationPort();
	}
	
	bool operator==(const TcpProxy &obj){
		return id == obj.id;
	}

private:
	// 客户端套接字 
	Socket* clientSocket;
	// 与目标服务器的TCP套接字 
	Socket destSocket;
	// 头部信息 
	char* headerBytes;
	//源ip 
	unsigned int srcIp;
	// 源端口 
	unsigned short srcPort;
	// 目标ip 
	unsigned int destIp;
	// 目标端口 
	unsigned short destPort;
	// 发送给客户端数据队列号 
	unsigned int mySeq;
	// 收到处理完成客户端数据队列号 
	unsigned int myAck;
	
	static int identification;
	static const int SYN_RCVD = 1;
	static const int ESTABLISHED = 2;
	static const int CLOSE_WAIT = 3;
	static const int LAST_ACK = 4;
	static const int CLOSED = 5;
	// IP/TCP通信状态 
	int state; 
	// 代理接收数据任务 
	Loop iloop;
	
	//未建立连接接收到的数据包容器 
	std::vector<CachePacket> cachePackets;
	//与目标服务器的TCP套接字是否建立连接 
	bool connected;
	// 代理创建时间 
	long createTime;
	// 最后刷新时间 
	long lastRefreshTime;
	// 错误消息 
	std::string  errorMsg;
	// 客户端id
	long clientId;
};
int TcpProxy::identification = 1;

#endif

