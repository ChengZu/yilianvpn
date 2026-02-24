#ifndef SOCKET_H
#define SOCKET_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstring>
#include <ctime>
#include "CommonMethods.h"

// 配置windows socket环境
#ifdef WIN32 // WIN32 宏, Linux宏不存在 
#include <WinSock2.h>
#include <Windows.h>
#pragma comment (lib, "WSOCK32.LIB")
#else
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#endif

class Socket
{
public:
	static const int TCP = 1;
	static const int UDP = 2;
	// 客户端最后刷新时间
	long lastClientRefreshTime;
	// 服务器最后刷新时间
	long lastServerRefreshTime;
	Socket()
	{
		closed = false;
		socket_fd = (int)INVALID_SOCKET;
		lastClientRefreshTime = std::time(NULL);
		lastServerRefreshTime = std::time(NULL);
	}

	Socket(int fd)
	{
		closed = false;
		socket_fd = fd;
		lastClientRefreshTime = std::time(NULL);
		lastServerRefreshTime = std::time(NULL);
	}

	Socket(int ip, int port, int protocol)
	{
		closed = false;
		lastClientRefreshTime = std::time(NULL);
		lastServerRefreshTime = std::time(NULL);
		this->ip = ip;
		this->port = port;
		this->protocol = protocol;
		int res = init();
		if(res == -1)
		{
			iClose();
		}
	}

	~Socket()
	{
	}

	void iClose()
	{
		closed = true;
		if (socket_fd != INVALID_SOCKET)
		{
#ifdef WIN32
			closesocket(socket_fd);
#else
			close(socket_fd);
#endif
			socket_fd = (int)INVALID_SOCKET;
		}
	}

	bool isClose()
	{
		return closed;
	}

	inline int socketRecv(char *buf, int size)
	{
		int ret = -1;
		lastServerRefreshTime = std::time(NULL);
#ifdef WIN32
		ret = recv(socket_fd, buf, size, 0);
#else
		ret = recv(socket_fd, buf, size, MSG_NOSIGNAL);
#endif
		if(ret > 0)
		{
			lastClientRefreshTime = std::time(NULL);
		}
		return ret;
	}

	inline int socketSend(char *buf, int size)
	{
		int ret = -1;
#ifdef WIN32
		ret = send(socket_fd, buf, size, 0);
#else
		ret = send(socket_fd, buf, size, MSG_NOSIGNAL);
#endif
		if(ret > 0)
		{
			lastServerRefreshTime = std::time(NULL);
		}
		return ret;
	}

	int getFd()
	{
		return socket_fd;
	}

private:

	int protocol;
	int ip;
	int port;
	int socket_fd;
	bool closed;

	int init()
	{
		int ret;
		// 配置一下windows socket 版本
		// 一定要加上这个，否者低版本的socket会出很多莫名的问题;
#ifdef WIN32
		WORD wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(2, 2);
		ret = WSAStartup(wVersionRequested, &wsaData);
		if (ret != 0)
		{
			printf("WSAStart up failed\n");
			return -1;
		}
#endif
		if(protocol == TCP)
		{
			socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}
		else if(protocol == UDP)
		{
			socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}
		else
		{
			perror("unhnown protocol");
			return -1;
		}

		if (socket_fd == INVALID_SOCKET)
		{
			perror("socket create fail");
			return -1;
		}
#ifdef WIN32
		u_long argp = 1;
		ioctlsocket(socket_fd, FIONBIO, &argp);
#else
		int flag;
		if ((flag = fcntl(socket_fd, F_GETFL, 0)) < 0)
			perror("socket get flag");
		flag |= O_NONBLOCK;
		if (fcntl(socket_fd, F_SETFL, flag) < 0)
			perror("socket set flag");
#endif

		// 配置一下要连接服务器的socket
		struct sockaddr_in sockaddr;
#ifdef WIN32
		std::string str = CommonMethods::ipIntToString(ip);
		char charip[str.size() + 1];
		std::strcpy(charip, str.c_str());
		sockaddr.sin_addr.S_un.S_addr = inet_addr(charip);
#else
		sockaddr.sin_addr.s_addr = htonl(ip);
#endif
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(port); // 连接信息要发送给监听socket;
		// 发送连接请求到我们服务端的监听socket;
		ret = connect(socket_fd, (const struct sockaddr *)&sockaddr, sizeof(sockaddr));
		if (ret != 0)
		{
			return  -2;//正在创建连接
		}

		return ret;
	}
};

#endif

