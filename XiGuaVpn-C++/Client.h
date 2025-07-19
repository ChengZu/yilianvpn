#ifndef CLIENT_H
#define CLIENT_H
#include "Loop.h"
#include "Task.h"
#include "Socket.h"
#include "TcpTunnel.h"
#include "UdpTunnel.h"

class Client: public Task
{
public:
	long myId;
	Client(){
	}
	
	Client(Socket* socket){
		UID++;
		myId = UID;
		closed = false;
		confirmUser = false;
		userInfoPacket = new char[Config::USER_INFO_HEADER_SIZE];
		userInfoPacketRead = 0;
		this->socket = socket;
		cacheBytes = NULL;
		cacheBytesSize = 0;
		tcpTunnel = TcpTunnel(myId, socket);
		udpTunnel = UdpTunnel(myId, socket);
		lastRefreshTime = std::time(NULL);
		iloop = Loop(this, (char*)"Client");
		iloop.join();
	}
	
	~Client(){
		closed = true;
		iloop.quit();
		socket->iClose();
		tcpTunnel.close();
		udpTunnel.close();
		if(cacheBytesSize > 0)
			delete[] cacheBytes;
		delete[] userInfoPacket;
		delete socket;
	}
	
	void close(){
		closed = true;
		iloop.quit();
		socket->iClose();
		tcpTunnel.close();
		udpTunnel.close();
		printf("[Client]client(%ld) closed.\n", myId);
	}
	
	bool isClose(){
		return closed || socket->isClose();
	}
	
	/*
	 * 处理IP数据包 TCP包让tcpTunnel处理 UDP包让udpTunnel处理 其他包不处理 并关闭客户端
	 */
	int processIPPacket(char* packet, int size){
		IPHeader header = IPHeader(packet, 0);
		char protocol = header.getProtocol();
		if (protocol == IPHeader::TCP) {
			tcpTunnel.processPacket(packet, size);
		} else if (protocol == IPHeader::UDP) {
			udpTunnel.processPacket(packet, size);
		} else {
			return -2; //无法处理的IP协议 
		}
		return 0;
	}
	
	/*
	 * 对接收的数据分包
	 */
	int processRecvBytes(char* bytes, int size){
		int ret = 0;
		if (this->cacheBytesSize > 0) {
			char* data = new char[this->cacheBytesSize + size];
			CommonMethods::arraycopy(this->cacheBytes, 0, data, 0, this->cacheBytesSize);
			CommonMethods::arraycopy(bytes, 0, data, this->cacheBytesSize, size);
			bytes = data;
			size = this->cacheBytesSize + size;
			
			delete[] this->cacheBytes;
			this->cacheBytesSize = 0;
		}

		if (size < IPHeader::IP4_HEADER_SIZE) {
			char* data = new char[size];
			CommonMethods::arraycopy(bytes, 0, data, 0, size);
			this->cacheBytes = data;
			this->cacheBytesSize = size;
			return 0;
		}

		IPHeader IpHeader = IPHeader(bytes, 0);
		int totalLength = IpHeader.getTotalLength();
		if(totalLength > Config::MUTE) return -1; //长度非法 

		if (totalLength < size) {
			ret = processIPPacket(bytes, totalLength);
			int nextDataSize = size - totalLength;
			char* data = new char[nextDataSize];
			CommonMethods::arraycopy(bytes, totalLength, data, 0, nextDataSize);
			processRecvBytes(data, nextDataSize);
			delete[] data;
		} else if (totalLength == size) {
			ret = processIPPacket(bytes, size);
		} else if (totalLength > size) {
			char* data = new char[size];
			CommonMethods::arraycopy(bytes, 0, data, 0, size);
			this->cacheBytes = data;
			this->cacheBytesSize = size;
		}
		return ret;
	}
	
	
	bool loop(){
		bool ret = false;
		if(isClose()) return true;
		if(confirmUser){
			char* bytes = new char[Config::MUTE];
			int size = socket->socketRecv(bytes, Config::MUTE);
			if (size > 0) {
				int res = processRecvBytes(bytes, size);
				lastRefreshTime = std::time(NULL);
				if(res == -1) {
					printf("[Client]client(%ld) recvive bad tcp/ip packet，closeing\n", myId);
					close();
					ret = true;
				}else if(res == -2){
					printf("[Client]client(%ld) recvive unknown protocol tcp/ip packet, closeing.\n", myId);
					close();
					ret = true;
				}
			}else if(size == 0){
				printf("[Client]client(%ld) lose connection, closeing.\n", myId);
				close();
				ret = true;
			}
			delete[] bytes;
		}else{
			int size = 0;
			// 读取头20字节，验证用户名密码 
			int needSize = Config::USER_INFO_HEADER_SIZE - userInfoPacketRead;
			char* bytes = new char[needSize];
			size = socket->socketRecv(bytes, needSize);
			if(size > 0){
				CommonMethods::arraycopy(bytes, 0, userInfoPacket, userInfoPacketRead, size);
				userInfoPacketRead += size;
			}else if (size == 0) {
				printf("[Client]client(%ld) lose connection, closeing.\n", myId);
				close();
				return true;
			}
			delete[] bytes;

			if (userInfoPacketRead < Config::USER_INFO_HEADER_SIZE) {
				return false;
			}

			IPHeader header = IPHeader(userInfoPacket, 0);
			// header.getSourceIP() 为用户名 
			// header.getDestinationIP() 为密码 
			if (header.getSourceIP() != Config::USER_NAME && header.getDestinationIP() != Config::USER_PASSWD) {
				printf("[Client]client(%ld) establish connection verify user name and password fail, closeing.\n", myId);
				socket->socketSend((char*)"Access denied", 13);
				close();
				return true;
			}
			confirmUser = true;
			printf("[Client]client(%ld) user:%u verify success, establish connection.\n", myId, header.getSourceIP());
		}
		return ret;
	}
	
	bool isExpire(){
		long now = std::time(NULL);
		return (now - lastRefreshTime) > Config::CLIENT_EXPIRE_TIME;
	}

private:
	// 客户端套接字 
	Socket* socket;
	// 缓存数据 
	char* cacheBytes;
	// 缓存数据大小 
	int cacheBytesSize;
	// TCP数据处理通道 
	TcpTunnel tcpTunnel;
	// UDP数据处理通道 
	UdpTunnel udpTunnel;
	// 已关闭状态 
	bool closed;
	// 已确认用户 
	bool confirmUser;
	// 用户信息头 
	char* userInfoPacket;
	// 用户信息头已读取 
	int userInfoPacketRead;
	// 最后刷新时间 
	long lastRefreshTime;
	// 接收客户端数据任务 
	Loop iloop;
	// 客户端ID生成 
	static long UID;
};

long Client::UID = 0;

#endif
