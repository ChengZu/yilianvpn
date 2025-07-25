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
#include "RingQueueBuffer.h"

class TcpProxy: public Proxy
{
public:
	TcpProxy(){
	}
	
	TcpProxy(long clientId, Socket* clientSocket, char* packet) : Proxy(clientId, clientSocket, packet){
		IPHeader ipHeader = IPHeader(muteBuffer, 0);
		TCPHeader tcpHeader = TCPHeader(muteBuffer, IPHeader::IP4_HEADER_SIZE);

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
		myWindow = 65535;
		clientWindow = 65535;
		clientFirstChar = 0;
		getClientFirstChar = false;
	}
	
	~TcpProxy(){
		close();
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
		int res = 0;
		if(connected){
			res = sendToBuffer(&destSocket, &clientRingQueueBuffer, bytes, size);
		}else{	
			if(!getClientFirstChar){
				if(size > 0){
					getClientFirstChar = true;
					clientFirstChar = bytes[0];
					int len = clientRingQueueBuffer.push(bytes + 1, size - 1);
					res = len + 1;
				}
			}else{
				res = clientRingQueueBuffer.push(bytes, size);
			}
			if((size - res) > 0) {
				printf("ERROR:[Proxy]proxy(%s) clientRingQueueBuffer no buffer space %d less %d.\n", toString().c_str(), clientRingQueueBuffer.availableWriteLength(), size - res);
			}	
		}
		myWindow = clientRingQueueBuffer.availableWriteLength();
		if(res < size){
			perror("[TcpProxy]socket error msg");
			printf("[TcpProxy]proxy(%s) send data to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
		}
		return res;
	}
	
	int sendToClient(char* bytes, int size){
		lastServerRefreshTime = std::time(NULL);
		int res = sendToBuffer(clientSocket, &myRingQueueBuffer, bytes, size);
		if(res < size){
			perror("[TcpProxy]socket error msg");
			printf("[TcpProxy]proxy(%s) send data to client fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
		}
		return res;
	}
	
	void sendRstToClient(std::string msg = "disconnect client by server") {
		close(msg);
		updateTCPBuffer(muteBuffer, TCPHeader::RST, mySeq, myAck, 0);
		sendToClient(muteBuffer, Proxy::TCP_HEADER_SIZE);
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
			sendRstToClient("close by fist code is syn");
		}
	}
	
	
	void processPacket(char* packet, int size) {
		IPHeader ipHeader = IPHeader(packet, 0);
		TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
		clientWindow = tcpHeader.getWindow();
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
		updateTCPBuffer(muteBuffer, (TCPHeader::SYN | TCPHeader::ACK), mySeq, myAck, 0);
		sendToClient(muteBuffer, Proxy::TCP_HEADER_SIZE);
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
				printf("[TcpProxy]proxy(%s)more packets, seq number %u max %u.\n", toString().c_str(), nextSeq, myAck);
			} else if(nextSeq < myAck){
				//printf("[TcpProxy]proxy(%s)repeat packets, seq number %u min %u.\n", toString().c_str(), nextSeq, myAck);
			}
		} else {
			printf("[TcpProxy]proxy(%s)miss packets，seq number %u max %u.\n", toString().c_str(), seq, myAck);
			myAck = tcpHeader.getSeqID() + 1;
		}
		updateTCPBuffer(muteBuffer, TCPHeader::ACK, mySeq, myAck, 0);
		// 发送数据收到ACK包 
		sendToClient(muteBuffer, Proxy::TCP_HEADER_SIZE);
	}
	
	void processRSTPacket(char* packet) {
		// printf("[TcpProxy]proxy(%s) recvive client rst packet, closeing.\n", toString().c_str()); 
		sendRstToClient("close because recvive client rst packet");
	}
	
	void processURGPacket(char* packet) {

	}

	void processPSHPacket(char* packet) {

	}

	void processFINPacket(char* packet) {
		// printf("[TcpProxy]proxy(%s) start closeing by client, state=%d.\n", toString().c_str(), state); 
		updateTCPBuffer(muteBuffer, TCPHeader::ACK, mySeq, myAck, 0);
		sendToClient(muteBuffer, Proxy::TCP_HEADER_SIZE);
		state = CLOSE_WAIT;	
		processCLOSEWAITACKPacket(packet); 
	}
	
	void processCLOSEWAITACKPacket(char* packet) {
		updateTCPBuffer(muteBuffer, (TCPHeader::FIN | TCPHeader::ACK), mySeq, myAck, 0);
		sendToClient(muteBuffer, Proxy::TCP_HEADER_SIZE);
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
	
	void sendBuffer(){
		//数据即可发送给服务器 
		int rlen = clientRingQueueBuffer.availableReadLength();
		if(rlen > 0){
			char* data = new char[rlen];
			clientRingQueueBuffer.poll(data, rlen);
			int res = sendData(&destSocket, data, rlen);
			if(res < rlen) {
				clientRingQueueBuffer.push(data + res, rlen - res);
			}
			delete[] data;
		}
		
		//数据即可发送给客户端 
		int rlen2 = myRingQueueBuffer.availableReadLength();
		if(rlen2 > 0){
			char* data = new char[rlen2];
			myRingQueueBuffer.poll(data, rlen2);
			int res = sendData(clientSocket, data, rlen2);
			if(res < rlen2) {
				myRingQueueBuffer.push(data + res, rlen2 - res);
			}
			delete[] data;
		}
	} 
	
	bool loop(){
		//printf("[TcpProxy]proxy(%s) loop.\n", toString().c_str());
		// 未检测到客户端断开，超时没数据也断开 
		// if(this->isExpire()) return true;
		// 与服务器未建立连接 
		if(!connected){
			if(getClientFirstChar){
				int res = destSocket.socketSend(&clientFirstChar, 1);
				if(res == 0){
					close("server disconnected");
					return true;
				}else if(res == 1){
					connected = true;
					//数据即可发送给服务器 
					sendBuffer();
				}
			}
			if(getClientFirstChar && (std::time(NULL) - createTime) > Config::TCP_CONNECT_TIMEOUT){
				// printf("[TcpProxy]proxy(%s) connection init timeout, socket errorno %d.\n", toString().c_str(), errno);
				close("connection init timeout");
				return true;
			}
			//return false;
		}else{
			sendBuffer();
		}
		

		int len = Config::MUTE - Proxy::TCP_HEADER_SIZE;
		int cw = clientWindow - myRingQueueBuffer.availableReadLength(); 
		len = cw < len ? cw : len;
		if(len <= 0){
			return false;
		}
		// 从服务器接收数据 
		int size = destSocket.socketRecv(muteBuffer + Proxy::TCP_HEADER_SIZE, len);
		if (size > 0) {
			connected = true;
			updateTCPBuffer(muteBuffer, TCPHeader::ACK, mySeq, myAck, size);
			// 转发给客户端 
			sendToClient(muteBuffer, Proxy::TCP_HEADER_SIZE + size);
			mySeq += size;
		}else if(size == 0){
			close("server disconnected");
			return true;
		}
		return false;
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
		tcpHeader.setWindow(myWindow);
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
	static const int SYN_RCVD = 1;
	static const int ESTABLISHED = 2;
	static const int CLOSE_WAIT = 3;
	static const int LAST_ACK = 4;
	static const int CLOSED = 5;
	int myWindow;
	int clientWindow;
	char clientFirstChar;
	bool getClientFirstChar;
};

#endif

