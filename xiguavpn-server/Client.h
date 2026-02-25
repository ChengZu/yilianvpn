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
		closed = false;
		this->socket = socket;
		cacheBytesSize = 0;
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
			if(clearNum > 0) printf("[Proxycontainer]client(%ld) proxy number max, cleaned up number %d, now proxy number %lu.\n", clientId, clearNum, proxys.size());
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
	
	int getTcpProxyNum()
	{
		int ret = 0;
		for (int i = 0; i < proxys.size(); i++)
		{
			Proxy *proxy = proxys[i];
			if(proxy->protocol == IPHeader::TCP)
			{
				ret++; 
			}
		}
		return ret;
	}
	
	int getUdpProxyNum()
	{
		int ret = 0;
		for (int i = 0; i < proxys.size(); i++)
		{
			Proxy *proxy = proxys[i];
			if(proxy->protocol == IPHeader::UDP)
			{
				ret++; 
			}
		}
		return ret;
	}
	
	int processCTRLPacket(char *packet, int size)
	{
		int res = 0;
		IPHeader revHeader = IPHeader(packet, 0);
		int code =  revHeader.getFlagsAndOffset();
		char msg[IPHeader::IP4_HEADER_SIZE];
        IPHeader ipheader = IPHeader(msg, 0);
        ipheader.setHeaderLength(IPHeader::IP4_HEADER_SIZE);
        int totalLength = IPHeader::IP4_HEADER_SIZE;
        ipheader.setTotalLength(totalLength);
        ipheader.setProtocol(Config::CTRL);
        ipheader.setFlagsAndOffset(code);
		if(code == 1000)
		{
			// header.getSourceIP() 为用户名
			// header.getDestinationIP() 为密码
			if (revHeader.getSourceIP() == Config::USER_NAME && revHeader.getDestinationIP() == Config::USER_PASSWD)
			{
				printf("[Client]client(%ld) user %u verify success, establish connection.\n", clientId, revHeader.getSourceIP());
				ipheader.setSourceIP(200);
			}
			else
			{
				printf("[Client]client(%ld) establish connection verify user name and password fail, closeing.\n", clientId);
				ipheader.setSourceIP(403);
				res = -3;
			} 
			
		}
		else if(code == 1001)
		{
			ipheader.setSourceIP(getTcpProxyNum());
        	ipheader.setDestinationIP(getUdpProxyNum());
		}
		
		int resTwo = socket.socketSend(msg, totalLength);
		if(resTwo == 0 || (resTwo > 0 && resTwo < totalLength))
		{
			printf("[Client]client(%ld) send lose connection, closeing.\n", clientId);
			res = -4;
		}
		
		return res;
	}

	/*
	 * 处理IP数据包 TCP包让tcpProxy处理 UDP包让udpProxy处理 其他包不处理 并关闭客户端
	 */
	int processIPPacket(char *packet, int size)
	{
		IPHeader header = IPHeader(packet, 0);

		char protocol = header.getProtocol();
		if (protocol == IPHeader::TCP)
		{
			processPacketToProxy(packet, size, IPHeader::TCP);
		}
		else if(protocol == IPHeader::UDP)
		{
			processPacketToProxy(packet, size, IPHeader::UDP);
		}
		else if(protocol == Config::CTRL)
		{
			return processCTRLPacket(packet, size);
		}
		else
		{
			printf("[Client]client(%ld) recvive unknown protocol packet, closeing.\n", clientId);
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
		if (cacheBytesSize > 0)
		{
			CommonMethods::arraycopy(bytes, 0, cacheBytes, cacheBytesSize, size);
			size = cacheBytesSize + size;
			cacheBytesSize = 0;
			ret = processRecvBytes(cacheBytes, size);
			return 0;
		}

		if (size < IPHeader::IP4_HEADER_SIZE)
		{
			CommonMethods::arraycopy(bytes, 0, cacheBytes, 0, size);
			cacheBytesSize = size;
			return 0;
		}

		IPHeader IpHeader = IPHeader(bytes, 0);
		int totalLength = IpHeader.getTotalLength();
		if(totalLength <= 0 || totalLength > Config::MUTE)
		{
			printf("[Client]client(%ld) recvive bad length packet, closeing\n", clientId);
			return -1; //长度非法
		}

		if (totalLength < size)
		{
			ret = processIPPacket(bytes, totalLength);
			int nextDataSize = size - totalLength;
			ret = processRecvBytes(bytes + totalLength, nextDataSize);
		}
		else if (totalLength == size)
		{
			ret = processIPPacket(bytes, size);
		}
		else
		{
			CommonMethods::arraycopy(bytes, 0, cacheBytes, 0, size);
			cacheBytesSize = size;
		}
		return ret;
	}


	bool loop()
	{
		//printf("[Client]%ldloop.\n", clientId);
		int size = socket.socketRecv(buffer, Config::MUTE);
		if (size > 0)
		{
			int res = processRecvBytes(buffer, size);
			if(res != 0)
			{
				close(); 
			}
		}
		else if(size == 0)
		{
			printf("[Client]client(%ld) rev lose connection, closeing.\n", clientId);
			close();
			return true;
		} 
		return false;
	}

	bool isExpire()
	{
		long now = std::time(NULL);
		return (now - socket.lastClientRefreshTime) > Config::CLIENT_EXPIRE_TIME || (now - socket.lastClientRefreshTime) > Config::CLIENT_EXPIRE_TIME;
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
	// 客户端ID生成
	static long UID;
	// 接收缓存
	char buffer[Config::MUTE];
	// 代理连接容器
	std::vector<Proxy *> proxys;
};

long Client::UID = 0;

#endif
