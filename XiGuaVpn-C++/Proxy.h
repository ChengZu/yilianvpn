#ifndef PROXY_H
#define PROXY_H
#include <ctime>
#include "Config.h"
#include "Task.h"
#include "IPHeader.h"
#include "TCPHeader.h"
#include "UDPHeader.h"
#include "Socket.h"

class Proxy: public Task
{
public:
    static const int TCP_HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + TCPHeader::TCP_HEADER_SIZE;
    static const int UDP_HEADER_SIZE = IPHeader::IP4_HEADER_SIZE + UDPHeader::UDP_HEADER_SIZE;

    Proxy()
    {
    }

    Proxy(long clientId, Socket *clientSocket, char *packet, char protocol)
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
        lastClientRefreshTime = std::time(NULL);
        lastServerRefreshTime = std::time(NULL);
        muteBuffer = new char[Config::MUTE];
        
        create_task(this);
    }

    ~Proxy()
    {
        closed = true;
        delete[] muteBuffer;
    }

    virtual void close(std::string msg = "") {}
    virtual bool isClosed()
    {
        return false;
    }
    virtual void processFisrtPacket(char *packet, int size) {}
    virtual void processPacket(char *packet, int size) {}
    virtual bool equal(char *packet)
    {
        return false;
    }

    int sendData(Socket *socket, char *bytes, int offset, int size, int waitTime = 30000)
    {
        int res = socket->socketSend(bytes + offset, size);
        res = res > 0 ? res : 0;
        while(res < size && waitTime > 0)
        {
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif // _WIN32
            int res2 = socket->socketSend(bytes + offset + res, size - res);
            if(res2 > 0) res += res2;
            waitTime--;
        }
        res = (res == 0) ? -1 : res;
        return res;
    }

    bool isExpire()
    {
        long now = std::time(NULL);
        return (now - lastClientRefreshTime) > Config::PROXY_EXPIRE_TIME || (now - lastServerRefreshTime) > Config::PROXY_EXPIRE_TIME;
    }


    std::string toString()
    {
        std::stringstream ss;
        ss << "[" << clientId << ":" << getId() << "][" << CommonMethods::ipIntToString(srcIp) << ":" << srcPort << "]->[" << CommonMethods::ipIntToString(destIp) << ":" << destPort << "]";
        return ss.str();
    }
protected:
    // 客户端套接字
    Socket *clientSocket;
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
    // 客户端最后刷新时间
    long lastClientRefreshTime;
    // 服务器最后刷新时间
    long lastServerRefreshTime;
    // 数据缓冲
    char *muteBuffer;
    // IP协议
    char protocol;
};

#endif
