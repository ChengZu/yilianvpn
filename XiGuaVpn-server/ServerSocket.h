#ifndef SERVERSOCKET_H
#define SERVERSOCKET_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstring>
#include "CommonMethods.h"

// 配置windows socket环境
#ifdef WIN32 // WIN32 宏, Linux宏不存在 
#include <WinSock2.h>
#include <Windows.h>
#pragma comment (lib, "WSOCK32.LIB")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#endif
// end


class ServerSocket
{
public:
    ServerSocket()
    {
    }

    ServerSocket(int ip, int port)
    {
        closed = false;
        int res = init(ip, port);
        if(res == -1)
        {
            perror("ServerSocket create error");
            iClose();
        }
    }

    ~ServerSocket()
    {
    }

    int getClientSocket()
    {
        // 等待客户介入进来;
        struct sockaddr_in c_address; // 客户端的IP地址;
#ifdef WIN32
        int address_len = sizeof(c_address);
#else
        socklen_t address_len = sizeof(c_address);
#endif

        // cs 是我们服务端为客户端创建的配对的socket;
        // c_address 就是我们客户端的IP地址和端口;
        int cs = accept(socket_fd, (struct sockaddr *)&c_address, &address_len);
        if (cs != -1)
        {
#ifdef WIN32
            u_long argp = 1;
            ioctlsocket(cs, FIONBIO, &argp);
#else
            int oldSocketFlag = fcntl(cs, F_GETFL, 0);
            int newSocketFlag = oldSocketFlag | O_NONBLOCK;
            fcntl(cs, F_SETFL,  newSocketFlag);
#endif
        }
        //printf("new client %s：%d\n", inet_ntoa(c_address.sin_addr), ntohs(c_address.sin_port));
        return cs;
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
        // 结束的时候也要清理
#ifdef WIN32
        WSACleanup();
#endif
    }

    bool isClose()
    {
        return closed;
    }

    int init(int ip, int port)
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

        // step1 创建一个监听的socket;
        socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // TCP
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
        // end

        // ip地址 + 端口,监听到哪个IP地址和端口上;
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
        sockaddr.sin_port = htons(port); // 127.0.0.1: port端口上;
        ret = bind(socket_fd, (const struct sockaddr *)&sockaddr, sizeof(sockaddr));
        if (ret != 0)
        {
            return -1;
        }
        // 开启监听
        ret = listen(socket_fd, 1);
        return ret;
    }

private:
    int socket_fd;
    bool closed;

};

#endif
