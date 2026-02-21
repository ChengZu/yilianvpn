#ifndef CLIENT_H
#define CLIENT_H
#include "Config.h"
#include "Socket.h"
#include "Task.h"
#include "Proxy.h"
#include "TcpProxy.h"
#include "UdpProxy.h"

class Client: public Task
{
public:
    long clientId;

    Client(Socket socket)
    {
        UID++;
        clientId = UID;
        name = "Client";
        closed = false;
        confirmUser = false;
        userInfoPacketRead = 0;
        this->socket = socket;
        cacheBytesSize = 0;
        lastRefreshTime = std::time(NULL);
        create_task(this);
    }

    ~Client()
    {
    }

    void close()
    {
        quit();
        closed = true;
        socket.iClose();
        closeAllProxy();
        printf("[Client]client(%ld) closed.\n", clientId);
    }

    bool isClose()
    {
        return closed;
    }

    /*
    * 接收数据包 建立新代理
    */
    void processPacketToProxy(char *packet, int size, char protocol)
    {
        // 清除已关闭代理，防止数据发送给已关闭代理
        clearCloseProxy();
        // 清除长时间未活动代理，减小内存使用
        if (proxys.size() > Config::CLIENT_MAX_PROXY)
        {
            int clearNum =  clearExpireProxy();
            if(clearNum > 0) printf("[Proxycontainer]client(%ld) proxy number max, cleaned up number %d, now proxy number %ld.\n", clientId, clearNum, proxys.size());
        }

        // 检查该代理是否创建
        for (int i = 0; i < proxys.size(); i++)
        {
            Proxy *proxy = proxys[i];
            if (proxy->equal(packet))
            {
                proxy->processPacket(packet, size);
                return;
            }
        }

        // 代理没创建，建立新代理
        if(protocol == IPHeader::TCP)
        {
            Proxy *proxy = new TcpProxy(clientId, socket, packet, protocol);
            proxys.push_back(proxy);
            proxy->processFisrtPacket(packet, size);
        }
        else if(protocol == IPHeader::UDP)
        {
            Proxy *proxy = new UdpProxy(clientId, socket, packet, protocol);
            proxys.push_back(proxy);
            proxy->processFisrtPacket(packet, size);
        }
    }

    /*
     * 清除不活动代理
     */
    int clearExpireProxy()
    {
        int ret = 0;
        for (int i = 0; i < proxys.size(); i++)
        {
            Proxy *proxy = proxys[i];
            if(proxy->isExpire())
            {
                proxys.erase(proxys.begin() + i);
                i--;
                ret++;
                delete proxy;
            }
        }
        return ret;
    }

    /*
     * 清除已关闭代理
     */
    int clearCloseProxy()
    {
        int ret = 0;
        for (int i = 0; i < proxys.size(); i++)
        {
            Proxy *proxy = proxys[i];
            if(proxy->isClose())
            {
                proxys.erase(proxys.begin() + i);
                i--;
                ret++;
                delete proxy;
            }
        }
        return ret;
    }

    /*
     * 清除所有代理
     */
    int closeAllProxy()
    {
        int ret = 0;
        for (int i = 0; i < proxys.size(); i++)
        {
            Proxy *proxy = proxys[0];
            if(!proxy->isClose())
            {
                proxy->close();
            }
            proxys.erase(proxys.begin());
            i--;
            ret++;
            delete proxy;
        }
        return ret;
    }

    /*
     * 处理IP数据包 TCP包让tcpProxy处理 UDP包让udpProxy处理 其他包不处理 并关闭客户端
     */
    int processIPPacket(char *packet, int size)
    {
        IPHeader header = IPHeader(packet, 0);
        if(header.getTotalLength() != size)
        {
            return -1; //非法长度
        }

        char protocol = header.getProtocol();
        if (protocol == IPHeader::TCP)
        {
            processPacketToProxy(packet, size, IPHeader::TCP);
        }
        else if(protocol == IPHeader::UDP)
        {
            processPacketToProxy(packet, size, IPHeader::UDP);
        }
        else
        {
            return -2; //无法处理的IP协议
        }
        return 0;
    }

    /*
     * 对接收的数据分包
     */
    int processRecvBytes(char *bytes, int size)
    {
        int ret = 0;
        if (this->cacheBytesSize > 0)
        {
            CommonMethods::arraycopy(bytes, 0, cacheBytes, cacheBytesSize, size);
            size = this->cacheBytesSize + size;
            this->cacheBytesSize = 0;
            ret = processRecvBytes(cacheBytes, size);
            return ret;
        }

        if (size < IPHeader::IP4_HEADER_SIZE)
        {
            CommonMethods::arraycopy(bytes, 0, cacheBytes, 0, size);
            this->cacheBytesSize = size;
            return 0;
        }

        IPHeader IpHeader = IPHeader(bytes, 0);
        int totalLength = IpHeader.getTotalLength();
        if(totalLength <= 0 || totalLength > Config::MUTE) return -1; //长度非法

        if (totalLength < size)
        {
            ret = processIPPacket(bytes, totalLength);
            int nextDataSize = size - totalLength;
            processRecvBytes(bytes + totalLength, nextDataSize);
        }
        else if (totalLength == size)
        {
            ret = processIPPacket(bytes, size);
        }
        else
        {
            CommonMethods::arraycopy(bytes, 0, cacheBytes, 0, size);
            this->cacheBytesSize = size;
        }
        return ret;
    }


    bool loop()
    {
        //printf("[Client]%ldloop.\n", myId);
        if(confirmUser)
        {
            int size = socket.socketRecv(buffer, Config::MUTE);
            if (size > 0)
            {
                int res = processRecvBytes(buffer, size);
                lastRefreshTime = std::time(NULL);
                if(res == -1)
                {
                    printf("[Client]client(%ld) recvive bad tcp/ip packet，closeing\n", clientId);
                    close();
                    return true;
                }
                else if(res == -2)
                {
                    printf("[Client]client(%ld) recvive unknown protocol tcp/ip packet, closeing.\n", clientId);
                    close();
                    return true;
                }
            }
            else if(size == 0)
            {
                printf("[Client]client(%ld) lose connection, closeing.\n", clientId);
                close();
                return true;
            }
        }
        else
        {
            int size = 0;
            // 读取头20字节，验证用户名密码
            int needSize = Config::USER_INFO_HEADER_SIZE - userInfoPacketRead;
            size = socket.socketRecv(userInfoPacket + userInfoPacketRead, needSize);
            if(size > 0)
            {
                userInfoPacketRead += size;
            }
            else if (size == 0)
            {
                printf("[Client]client(%ld) disconnected, closeing.\n", clientId);
                close();
                return true;
            }

            if (userInfoPacketRead < Config::USER_INFO_HEADER_SIZE)
            {
                return false;
            }

            IPHeader header = IPHeader(userInfoPacket, 0);
            // header.getSourceIP() 为用户名
            // header.getDestinationIP() 为密码
            if (header.getSourceIP() != Config::USER_NAME && header.getDestinationIP() != Config::USER_PASSWD)
            {
                printf("[Client]client(%ld) establish connection verify user name and password fail, closeing.\n", clientId);
                socket.socketSend((char *)"Access denied", 13);
                close();
                return true;
            }
            confirmUser = true;
            printf("[Client]client(%ld) user %u verify success, establish connection.\n", clientId, header.getSourceIP());
        }
        return false;
    }

    bool isExpire()
    {
        long now = std::time(NULL);
        return (now - lastRefreshTime) > Config::CLIENT_EXPIRE_TIME;
    }

private:
    // 客户端套接字
    Socket socket;
    // 缓存数据
    char cacheBytes[Config::MUTE * 2];
    // 缓存数据大小
    int cacheBytesSize;
    // 已关闭状态
    bool closed;
    // 已确认用户
    bool confirmUser;
    // 用户信息头
    char userInfoPacket[Config::USER_INFO_HEADER_SIZE];
    // 用户信息头已读取
    int userInfoPacketRead;
    // 最后刷新时间
    long lastRefreshTime;
    // 客户端ID生成
    static long UID;
    // 接收缓存
    char buffer[Config::MUTE];
    // 代理连接容器
    std::vector<Proxy *> proxys;
};

long Client::UID = 0;

#endif
