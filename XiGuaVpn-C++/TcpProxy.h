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
	
	TcpProxy(Socket* socket, char* packet){
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
		CommonMethods::arraycopy(packet, 0, headerBytes, 0, IPHeader::IP4_HEADER_SIZE);
		CommonMethods::arraycopy(packet, ipHeaderLen, headerBytes, IPHeader::IP4_HEADER_SIZE, TCPHeader::TCP_HEADER_SIZE);
		ipHeader = IPHeader(headerBytes, 0);
		tcpHeader = TCPHeader(headerBytes, IPHeader::IP4_HEADER_SIZE);
		ipHeader.setSourceIP(destIp);
		tcpHeader.setSourcePort(destPort);
		ipHeader.setDestinationIP(srcIp);
		tcpHeader.setDestinationPort(srcPort);
		ipHeader.setFlagsAndOffset(0);
		tcpHeader.setWindow(65535);
		tcpHeader.setUrp(0);
		tcpHeader.setHeaderLength(20);
		state = 0;
		iClose = false;
		connected = false;
		createTime = std::time(NULL);
		lastRefreshTime = std::time(NULL);
		
		this->destSocket = Socket(destIp, destPort);
		
		iloop = Loop(this, (char*)"TcpProxy");
		iloop.join();
	}
	
	~TcpProxy(){
		iClose = true;
		iloop.quit();
		destSocket.iClose();
		delete[] headerBytes;
		for (int i = 0; i < cachePackets.size(); i++) {
			delete[] cachePackets[i].data;
		}
	}
	
	void destroy() {
		close();
		state = CLOSED;
	}
	
	void close(){
		iClose = true;
		iloop.quit();
		destSocket.iClose();
		//printf("TcpProxy(%ld) closed.\n", id);
	}
	
	bool finishClose(){
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
			if(res == 0){
				disConnectClient();
			}else if(res == -1) {
				//printf("TcpProxy(%ld) error: sendToServer(%s:%d) retry=%d.\n", id, CommonMethods::ipIntToString(destIp).c_str(), destPort, retry);
				//perror("error msg");
				if(retry > 0){
					sendToServer(bytes, size, offset, retry - 1);
				}
			}else if((res > 0) && (res < size)){
				sendToServer(bytes, size, (offset + res));
			}
			int dataSize = 0;
			if(res > 0) dataSize = res;
			if(retry == 0 && (offset + dataSize) != size){
				disConnectClient();
				perror("error msg");
				printf("TcpProxy(%ld) error: sendToServer packet min %d @ %d.\n", id, (offset + dataSize), size);
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
		//如果发送异常，放弃这个TCP连接 
		if(res == 0){
			close();
			state = CLOSED;
		}else if(res == -1) {
			//perror("error msg");
			//printf("TcpProxy(%ld) error: sendToClient retry=%d.\n", id, retry);
			if(retry > 0){
				sendToClient(bytes, size, offset, retry - 1);
			}
		}else if((res > 0) && (res < size)){
			sendToClient(bytes, size, (offset + res));
		}
		int dataSize = 0;
		if(res > 0) dataSize = res;
		if(retry == 0 && (offset + dataSize) != size){
			//close();
			//state = CLOSED;
			perror("error msg");
			printf("TcpProxy(%ld) error: sendToClient packet min %d @ %d.\n", id, (offset + dataSize), size);
		}
		return res;
	}
	
	void processPacket(std::vector<TcpProxy*> *tcpProxys, char* packet, bool newTcpProxy) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, IPHeader::IP4_HEADER_SIZE);
		int flags = tcpHeader.getFlag();
		if(newTcpProxy && (flags & TCPHeader::SYN) != TCPHeader::SYN) {
			if((flags & TCPHeader::RST) != TCPHeader::RST){
				updateTCPBuffer(headerBytes, (char)TCPHeader::RST, mySeq, myAck, 0);
				sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
			}
			destroy();
			return;
		}
		if(newTcpProxy) {
			tcpProxys->push_back(this);
		}
		
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
			printf("Client(%ld) tcpProxy(%s:%d) 未处理标志位%d{%s}.\n", id, CommonMethods::ipIntToString((int)ipHeader.getDestinationIP()).c_str(), tcpHeader.getDestinationPort(), flags, tcpHeader.toString().c_str());
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
			printf("TcpProxy(%ld%s) Connect closed, state=%d.\n", id, toString().c_str(), state);
			break;
		case ESTABLISHED:
			processESTABLISHEDACKPacket(packet);
			break;
		default:
			printf("TcpProxy(%ld) No function deal ACK packet.\n", id);
			break;
		}
	}
	
	void processSYNRCVDACKPacket(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		if (tcpHeader.getSeqID() == myAck && tcpHeader.getAckID() == mySeq) {
			state = ESTABLISHED;
		}else {
			printf("TcpProxy(%ld) Bad SYNRCVDACK.\n", id);
		}
	}
	
	void processESTABLISHEDACKPacket(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		int dataSize = ipHeader.getTotalLength() - ipHeader.getHeaderLength() - tcpHeader.getHeaderLength();
		int sequenceNumber = tcpHeader.getSeqID();	
		if (sequenceNumber == myAck) { 
			//printf("TcpProxy(%ld) 序列号匹配.\n", id);
			if (dataSize > 0) {
				char* data = new char[dataSize];
				CommonMethods::arraycopy(packet, (ipHeader.getHeaderLength() + tcpHeader.getHeaderLength()), data, 0, dataSize);
				sendToServer(data, dataSize);
				delete[] data;
				//下一个序列号
				myAck += dataSize;
				updateTCPBuffer(headerBytes, TCPHeader::ACK, mySeq, myAck, 0);
				//发送数据收到ACK包 
				sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
			}
		} else if (sequenceNumber < myAck) {	
			int nextSeq = sequenceNumber + dataSize;
			if (nextSeq > myAck) {
				printf("TcpProxy(%ld) ACK seq %u max %u, 数据多包不处理.\n", id, nextSeq, myAck);
			} else {
				//printf("TcpProxy(%ld) ACK seq %u min %u, 数据重传不处理.\n", id, nextSeq, myAck);
			}
		} else {
			printf("TcpProxy(%ld) ACK seq %u max %u, 数据漏包不处理.\n", id, sequenceNumber, myAck);
		}
	}
	
	void processRSTPacket(char* packet) {
		//printf("TcpProxy(%ld) RST, 关闭连接.\n", id);
		close();
	}
	
	void processURGPacket(char* packet) {

	}

	void processPSHPacket(char* packet) {

	}

	void processFINPacket(char* packet) {
		//printf("TcpProxy(%ld) 开始被动关闭, state=%d.\n", id, state);
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
			state = CLOSED;
			//printf("TcpProxy(%ld) LAST_ACK succeed, 关闭连接成功, seq %u:%u, ack %u:%u.\n", id, seq, mySeq, ack, myAck);
		} else {
			printf("TcpProxy(%ld) LAST_ACK failed, 队列号不匹配，seq %u:%u, ack %u:%u.\n", id, seq, mySeq, ack, myAck);
		}
	}
	
	void disConnectClient() {
		//printf("TcpProxy(%ld) 开始主动关闭, state=%d.\n", id, state);
		close();
		updateTCPBuffer(headerBytes, TCPHeader::RST, mySeq, myAck, 0);
		sendToClient(headerBytes, TcpProxy::HEADER_SIZE);
	}
	
	bool loop(){
		bool ret = false;
		
		if(!connected){
			if(cachePackets.size() > 0){
				int res = destSocket.socketSend(cachePackets[0].data, cachePackets[0].size);
				if(res == 0){
					disConnectClient();
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
					printf("TcpProxy(%ld) connect(%s:%d) init fail, %d @ %d.\n", id, CommonMethods::ipIntToString(destIp).c_str(), destPort, res, cachePackets[0].size);
					disConnectClient();
					return true;
				}
			}
			if((std::time(NULL) - createTime) > Config::TCP_CONNECT_TIMEOUT){
				//printf("TcpProxy(%ld) init connect(%s:%d) timeout, errno %d.\n", id, CommonMethods::ipIntToString(destIp).c_str(), destPort, errno);
				disConnectClient();
				return true;
			}
			return false;
		}

		//从服务器接收数据 
		char* bytes = new char[Config::MUTE - TcpProxy::HEADER_SIZE];
		int size = destSocket.socketRecv(bytes, Config::MUTE - TcpProxy::HEADER_SIZE);
		if (size > 0) {
			int packetLen = TcpProxy::HEADER_SIZE + size;
			char* packet = new char[packetLen];
			CommonMethods::arraycopy(headerBytes, 0, packet, 0, TcpProxy::HEADER_SIZE);
			CommonMethods::arraycopy(bytes, 0, packet, TcpProxy::HEADER_SIZE, size);
			updateTCPBuffer(packet, TCPHeader::ACK, mySeq, myAck, size);
			sendToClient(packet, packetLen);
			mySeq += size;
			delete[] packet;
		}else if(size == 0){
			disConnectClient();
			ret = true;
		}	
		delete[] bytes;
		return ret;
	}
	
	void updateTCPBuffer(char* packet, char flag, int seq, int ack, int dataSize){
		IPHeader ipHeader = IPHeader(packet, 0);
		identification++;
		ipHeader.setIdentification(identification);
		TCPHeader tcpHeader = TCPHeader(packet, IPHeader::IP4_HEADER_SIZE);
		tcpHeader.setFlag(flag);
		tcpHeader.setSeqID(seq);
		tcpHeader.setAckID(ack);
		ipHeader.setTotalLength(TcpProxy::HEADER_SIZE + dataSize);
		tcpHeader.ComputeTCPChecksum(ipHeader);
	}
	
	int getState(){
		return state;
	}
	
	std::string toString(){
		std::stringstream ss;
    	ss <<"["<< CommonMethods::ipIntToString(destIp) << ":" << destPort << "]->[" << CommonMethods::ipIntToString(srcIp) << ":" << srcPort <<"]";
    	return ss.str();
	}
	
	bool isExpire(){
		long now = std::time(NULL);
		return (now - lastRefreshTime) > Config::TCPPROXY_EXPIRE_TIME;
	}
	
	bool equal(char* packet) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == tcpHeader.getSourcePort()
				&& destIp == ipHeader.getDestinationIP()
				&& destPort == tcpHeader.getDestinationPort();
	}
	
	bool operator==(const TcpProxy &obj){
		return id == obj.id;
	}

private:
	//客户端套接字 
	Socket* clientSocket;
	//与目标服务器的TCP套接字
	Socket destSocket;
	//头部信息
	char* headerBytes;
	//源ip
	unsigned int srcIp;
	//源端口
	unsigned short srcPort;
	//目标ip
	unsigned int destIp;
	//目标端口
	unsigned short destPort;
	//发送给客户端数据队列号
	unsigned int mySeq;
	//收到处理完成客户端数据队列号
	unsigned int myAck;
	
	static int identification;
	static const int SYN_RCVD = 1;
	static const int ESTABLISHED = 2;
	static const int CLOSE_WAIT = 3;
	static const int LAST_ACK = 4;
	static const int CLOSED = 5;
	//IP/TCP通信状态 
	int state; 
	//代理接收数据任务 
	Loop iloop;
	
	//未建立连接接收到的数据包容器 
	std::vector<CachePacket> cachePackets;
	//是否关闭 
	bool iClose;
	//与目标服务器的TCP套接字是否建立连接 
	bool connected;
	//代理创建时间 
	long createTime;
	//最后刷新时间 
	long lastRefreshTime;
	//出错原因
	char* errorMsg; 
};
int TcpProxy::identification = 1;

#endif

