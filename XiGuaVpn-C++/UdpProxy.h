#ifndef UDPPROXY_H
#define UDPPROXY_H
#include <ctime>
#include "Config.h"
#include "Task.h"
#include "IPHeader.h"
#include "UDPHeader.h"
#include "Proxy.h"
#include "Socket.h"


class UdpProxy: public Proxy
{
public:
    UdpProxy()
    {
    }

    UdpProxy(long clientId, Socket clientSocket, char *packet, char protocol) : Proxy(clientId, clientSocket, packet, protocol)
    {
        IPHeader ipHeader = IPHeader(buffer, 0);
        UDPHeader udpHeader = UDPHeader(buffer, IPHeader::IP4_HEADER_SIZE);

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

        name = "UdpProxy";
    }

    ~UdpProxy()
    {
    }

    void close(std::string msg = "")
    {
        quit();
        errorMsg = msg;
        closed = true;
        destSocket.iClose();
        // printf("[UdpProxy](%s) closed, the msg is %s.\n", toString().c_str(), errorMsg.c_str());
    }

    bool isClose()
    {
        return closed;
    }

    int sendToServer(char *bytes, int size)
    {
        lastClientRefreshTime = std::time(NULL);
        int res = sendData(destSocket, bytes, 0, size);
        if(res < size)
        {
            perror("[UdpProxy]socket error msg");
            printf("[UdpProxy](%s) send data to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
        }
        return res;
    }

    int sendToClient(char *bytes, int size)
    {
        lastServerRefreshTime = std::time(NULL);
        int res = sendData(clientSocket, bytes, 0, size);
        if(res < size)
        {
            perror("[UdpProxy]socket error msg");
            printf("[UdpProxy](%s) send data to client fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
        }
        return res;
    }

    void processFisrtPacket(char *packet, int size)
    {
        destSocket = Socket(destIp, destPort, Socket::UDP);
        if(!destSocket.isClose())
        {
            processPacket(packet, size);
        }
        else
        {
            close("socket create fail");
        }
    }

    void processPacket(char *packet, int size)
    {
        IPHeader ipHeader = IPHeader(packet, 0);
        int headerLen = ipHeader.getHeaderLength() + UDPHeader::UDP_HEADER_SIZE;
        int dataSize = ipHeader.getTotalLength() - headerLen;
        if(dataSize > 0)
        {
            // 转发给服务器
            sendToServer(packet + headerLen, dataSize);
        }
    }

    bool loop()
    {
        // printf("[UdpProxy](%s) loop.\n", toString().c_str());
        // 从服务器接收数据
        int size = destSocket.socketRecv(buffer + Proxy::UDP_HEADER_SIZE, Config::MUTE - Proxy::UDP_HEADER_SIZE);
        if (size > 0)
        {
            updateUDPBuffer(buffer, size);
            // 转发给客户端
            sendToClient(buffer, Proxy::UDP_HEADER_SIZE + size);
        }
        else if(size == 0)
        {
            close();
            return true;
        }
        return false;
    }

    bool equal(char *packet)
    {
        IPHeader ipHeader = IPHeader(packet, 0);
        UDPHeader udpHeader = UDPHeader(packet, ipHeader.getHeaderLength());
        return protocol == ipHeader.getProtocol() && srcIp == ipHeader.getSourceIP() && srcPort == udpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP()	&& destPort == udpHeader.getDestinationPort();
    }

    void updateUDPBuffer(char *packet, int size)
    {
        IPHeader ipHeader = IPHeader(packet, 0);
        UDPHeader udpHeader = UDPHeader(packet, ipHeader.getHeaderLength());
        ipHeader.setTotalLength(Proxy::UDP_HEADER_SIZE + size);
        udpHeader.setTotalLength(UDPHeader::UDP_HEADER_SIZE + size);
        udpHeader.ComputeUDPChecksum(ipHeader);
    }
};

#endif
