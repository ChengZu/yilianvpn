#ifndef PROXY_H
#define PROXY_H
#include <ctime>
#include "Config.h"
#include "Socket.h"
#include "Task.h"
#include "IPHeader.h"
#include "TCPHeader.h"
#include "UDPHeader.h"

class Proxy: public Task
{
public:
	// IP协议
	char protocol;
	static const int TCP_HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + TCPHeader::TCP_HEADER_SIZE;
	static const int UDP_HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + UDPHeader::UDP_HEADER_SIZE;

	Proxy()
	{
	}

	Proxy(long clientId, Socket clientSocket, char *packet, char protocol)
	{
		this->clientId = clientId;
		this->clientSocket = clientSocket;
		this->protocol = protocol;
		IPHeader ipHeader = IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		srcIp = ipHeader.getSourceIP();
		destIp = ipHeader.getDestinationIP();

		if (protocol == IPHeader::TCP)
		{
			TCPHeader tcpHeader = TCPHeader(packet, ipHeaderLen);
			srcPort = tcpHeader.getSourcePort();
			destPort = tcpHeader.getDestinationPort();
		}
		else if (protocol == IPHeader::UDP)
		{
			UDPHeader udpHeader = UDPHeader(packet, ipHeaderLen);
			srcPort = udpHeader.getSourcePort();
			destPort = udpHeader.getDestinationPort();
		}
		else
		{
			close();
			return;
		}

		closed = false;
		createTime = std::time(NULL);
	}

	~Proxy()
	{
	}

	virtual void close(std::string msg = "") {}
	virtual bool isClose()
	{
		return false;
	}
	virtual void processFisrtPacket(char *packet, int size) {}
	virtual void processPacket(char *packet, int size) {}
	virtual bool equal(char *packet)
	{
		return false;
	}

	int sendData(Socket &socket, char *bytes, int offset, int size, int maxTimeout = 30)
	{
		int res = socket.socketSend(bytes + offset, size);
		if(res == 0)
		{
			close();
			return res;
		}
		res = res > 0 ? res : 0;
		if(res < size)
		{
			printf("[Proxy](%s) id %ld can not write.\n", toString().c_str(), getId());
		}
		while(res < size && maxTimeout > 0)
		{
			fd_set writefds;
			int fd = socket.getFd();
			struct timeval timeout = {30, 0};

			// 初始化读集合
			FD_ZERO(&writefds);

			// 添加文件描述符到读集合
			FD_SET(fd, &writefds);

			// 调用 select() 监视读集合 int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
			select(fd + 1, NULL, &writefds, NULL, &timeout);

			// 检查文件描述符是否准备好读取
			if (FD_ISSET(fd, &writefds))
			{
				// 文件描述符准备好了，可以进行写操作
				int resTwo = socket.socketSend(bytes + offset + res, size - res);
				if(resTwo == 0)
				{
					close();
					return resTwo;
				}
				if(resTwo > 0) res += resTwo;
			}

			// 从集合中删除文件描述符
			FD_CLR(fd, &writefds);
			maxTimeout -= timeout.tv_sec;
		}
		return res;
	}

	bool isExpire()
	{
		long now = std::time(NULL);
		return (now - clientSocket.lastClientRefreshTime) > Config::PROXY_EXPIRE_TIME || (now - clientSocket.lastServerRefreshTime) > Config::PROXY_EXPIRE_TIME;
	}


	std::string toString()
	{
		std::stringstream ss;
		ss << "[" << clientId << ":" << getId() << "][" << CommonMethods::ipIntToString(srcIp) << ":" << srcPort << "]->[" << CommonMethods::ipIntToString(destIp) << ":" << destPort << "]";
		return ss.str();
	}
protected:
	// 客户端套接字
	Socket clientSocket;
	// 连接目标服务器的套接字
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
	// 错误消息
	std::string  errorMsg;
	// 客户端id
	long clientId;
	// 创建时间
	long createTime;
	// 数据缓冲
	char buffer[Config::MUTE];
};

#endif

