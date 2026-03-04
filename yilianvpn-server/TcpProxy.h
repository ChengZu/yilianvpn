#ifndef TCPPROXY_H
#define TCPPROXY_H
#include <ctime>
#include "CommonMethods.h"
#include "Config.h"
#include "Socket.h"
#include "Task.h"
#include "IPHeader.h"
#include "TCPHeader.h"
#include "Proxy.h"

class DataList
{
public:
    DataList(char *data, int size)
    {
        this->data = data;
        this->size = size;
    }
    ~DataList()
    {
        size = 0;
        delete[] data;
    }
    char *data;
    int size;
};

class TcpProxy: public Proxy
{
public:
    TcpProxy()
    {
    }

    TcpProxy(long clientId, Socket clientSocket, char *packet) : Proxy(clientId, clientSocket)
    {
        protocol = IPHeader::TCP;
        IPHeader oldIPHeader = IPHeader(packet, 0);
        int ipHeaderLen = oldIPHeader.getHeaderLength();
        srcIp = oldIPHeader.getSourceIP();
        destIp = oldIPHeader.getDestinationIP();
        TCPHeader oldTCPHeader = TCPHeader(packet, ipHeaderLen);
        srcPort = oldTCPHeader.getSourcePort();
        destPort = oldTCPHeader.getDestinationPort();

        IPHeader ipHeader = IPHeader(buffer, 0);
        TCPHeader tcpHeader = TCPHeader(buffer, IPHeader::IP4_HEADER_SIZE);
        ipHeader.setHeaderLength(IPHeader::IP4_HEADER_SIZE);
        ipHeader.setTos(oldIPHeader.getTos());
        ipHeader.setIdentification(0);
        ipHeader.setFlagsAndOffset(oldIPHeader.getFlagsAndOffset());
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
    }

    ~TcpProxy()
    {
    }

    void close(std::string msg = "close done")
    {
        quit();
        errorMsg = msg;
        closed = true;
        destSocket.iClose();
        // printf("[TcpProxy](%s) closed, the msg is %s.\n", toString().c_str(), errorMsg.c_str());
    }

    bool isClose()
    {
        return closed;
    }

    int sendToServer(char *bytes, int size)
    {
        int res = size;
        if(connected)
        {
            res = sendData(destSocket, bytes, 0, size);
        }
        else
        {
            char *data = new char[size];
            CommonMethods::arraycopy(bytes, 0, data, 0, size);
            DataList *list = new DataList(data, size);
            dataList.push_back(list);
        }

        if(res < size)
        {
            perror("[TcpProxy]socket error msg");
            printf("[TcpProxy](%s) send data to server fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
            close();
        }
        return res;
    }

    int sendToClient(char *bytes, int size)
    {
        int res = sendData(clientSocket, bytes, 0, size);
        if(res < size)
        {
            perror("[TcpProxy]socket error msg");
            printf("[TcpProxy](%s) send data to client fail, total %d bytes, success send %d bytes.\n", toString().c_str(), size, res);
            close();
        }
        return res;
    }

    void processFisrtPacket(char *packet, int size)
    {
        mIpHeader = IPHeader(packet, 0);
        mTcpHeader = TCPHeader(packet, mIpHeader.getHeaderLength());
        serverSeq = 0;
        clientSeq = mTcpHeader.getSeqID() + 1;
        int flags = mTcpHeader.getFlag();

        if((flags & TCPHeader::SYN) == TCPHeader::SYN)
        {
            destSocket = Socket(destIp, destPort, Socket::TCP);
            if(!destSocket.isClose())
            {
                processPacket(packet, size);
            }
            else
            {
                close("socket create fail");
                printf("[TcpProxy](%s) socket create fail.\n", toString().c_str());
            }
        }
        else
        {
            //printf("[TcpProxy](%s) recvive client first packet(%s) is not syn, flags is %d.\n", toString().c_str(), tcpHeader.toString().c_str(), flags);
            close("close by fist code is not syn");
        }
    }


    void processPacket(char *packet, int size)
    {
        mIpHeader = IPHeader(packet, 0);
        mTcpHeader = TCPHeader(packet, mIpHeader.getHeaderLength());
        clientWindow = mTcpHeader.getWindow();
        int flags = mTcpHeader.getFlag();

        if ((flags | TCPHeader::SYN) == TCPHeader::SYN)
        {
            processSYNPacket(packet);
        }
        else if (flags == (TCPHeader::FIN | TCPHeader::ACK))
        {
            processFINPacket(packet);
        }
        else if ((flags | TCPHeader::ACK) == TCPHeader::ACK)
        {
            processACKPacket(packet);
        }
        else if (flags == (TCPHeader::ACK | TCPHeader::RST))
        {
            processACKPacket(packet);
            processRSTPacket(packet);
        }
        else if (flags == (TCPHeader::ACK | TCPHeader::PSH))
        {
            processACKPacket(packet);
        }
        else if (flags == (TCPHeader::ACK | TCPHeader::PSH | TCPHeader::FIN))
        {
            processACKPacket(packet);
            processFINPacket(packet);
        }
        else if ((flags | TCPHeader::RST) == TCPHeader::RST)
        {
            processRSTPacket(packet);
        }
        else if ((flags | TCPHeader::PSH) == TCPHeader::PSH)
        {
            processPSHPacket(packet);
        }
        else if ((flags | TCPHeader::URG) == TCPHeader::URG)
        {
            processURGPacket(packet);
        }
        else
        {
            printf("[TcpProxy](%s), packet flags %d program unable to process.\n", toString().c_str(), flags);
        }
    }

    void processSYNPacket(char *packet)
    {
        serverSeq = 0;
        clientSeq = mTcpHeader.getSeqID() + 1;
        updateTCPBuffer(buffer, (TCPHeader::SYN | TCPHeader::ACK), serverSeq, clientSeq, 0);
        sendToClient(buffer, Proxy::TCP_HEADER_SIZE);
        state = SYN_WAIT_ACK;
        serverSeq += 1;
    }

    void processACKPacket(char *packet)
    {
        switch (state)
        {
        case SYN_WAIT_ACK:
            processSYNWAITACKPacket(packet);
            break;
        case CLOSE_WAIT:
            processCLOSEWAITACKPacket(packet);
            break;
        case LAST_ACK:
            processLASTACKPacket(packet);
            break;
        case CLOSED:
            printf("[TcpProxy](%s) process ack packet but state closed, state=%d.\n", toString().c_str(), state);
            break;
        case ESTABLISHED:
            processESTABLISHEDACKPacket(packet);
            break;
        default:
            printf("[TcpProxy](%s) process ack packet but state abnormal, state=%d.\n", toString().c_str(), state);
            break;
        }
    }

    void processSYNWAITACKPacket(char *packet)
    {
        if (mTcpHeader.getSeqID() == clientSeq && mTcpHeader.getAckID() == serverSeq)
        {
            state = ESTABLISHED;
        }
        else
        {
            //printf("[TcpProxy](%s) SYN_WAIT_ACK fail.\n", toString().c_str());
            close("syn wait check ack fail");
        }
    }

    void processESTABLISHEDACKPacket(char *packet)
    {
        int headerLength = mIpHeader.getHeaderLength() + mTcpHeader.getHeaderLength();
        int dataSize = mIpHeader.getTotalLength() - headerLength;
        unsigned int seq = mTcpHeader.getSeqID();
        if (seq == clientSeq)
        {
            //printf("[TcpProxy](%s) recvive client ack queue number match.\n", toString().c_str());
            if(dataSize > 0)
            {
                sendToServer(packet + headerLength, dataSize);
                // 下一个序列号
                clientSeq += dataSize;
                // 发送数据收到ACK包
                updateTCPBuffer(buffer, TCPHeader::ACK, serverSeq, clientSeq, 0);
                sendToClient(buffer, Proxy::TCP_HEADER_SIZE);
            }
        }
        else if (seq < clientSeq)
        {
            unsigned int nextSeq = seq + dataSize;
            if (nextSeq > clientSeq)
            {
                printf("[TcpProxy](%s)more packets, seq number %u max %u.\n", toString().c_str(), nextSeq, clientSeq);
            }
            else if(nextSeq < clientSeq)
            {
                //printf("[TcpProxy]proxy(%s)repeat packets, seq number %u min %u.\n", toString().c_str(), nextSeq, clientSeq);
            }
        }
        else
        {
            printf("[TcpProxy](%s)miss packets, seq number %u max %u.\n", toString().c_str(), seq, clientSeq);
        }
    }

    void processRSTPacket(char *packet)
    {
        // printf("[TcpProxy](%s) recvive client rst packet, closeing.\n", toString().c_str());
        updateTCPBuffer(buffer, TCPHeader::RST, serverSeq, clientSeq, 0);
        sendToClient(buffer, Proxy::TCP_HEADER_SIZE);
    }

    void processURGPacket(char *packet)
    {

    }

    void processPSHPacket(char *packet)
    {

    }

    void processFINPacket(char *packet)
    {
        // printf("[TcpProxy](%s) start closeing by client, state=%d.\n", toString().c_str(), state);
        updateTCPBuffer(buffer, TCPHeader::ACK, serverSeq, clientSeq, 0);
        sendToClient(buffer, Proxy::TCP_HEADER_SIZE);
        state = CLOSE_WAIT;
        processCLOSEWAITACKPacket(packet);
    }

    void processCLOSEWAITACKPacket(char *packet)
    {
        updateTCPBuffer(buffer, (TCPHeader::FIN | TCPHeader::ACK), serverSeq, clientSeq, 0);
        sendToClient(buffer, Proxy::TCP_HEADER_SIZE);
        state = LAST_ACK;
    }

    void processLASTACKPacket(char *packet)
    {
        unsigned int ack = mTcpHeader.getAckID() - 1;
        unsigned int seq = mTcpHeader.getSeqID() - 1;
        if (ack == serverSeq && seq == clientSeq)
        {
            // printf("[TcpProxy](%s) LAST_ACK confirm success, seq %u:%u, ack %u:%u, close success.\n", toString().c_str(), seq, clientSeq, ack, serverSeq);
            state = CLOSED;
            // 关闭完成, 释放资源
            close();
        }
        else
        {
            //printf("[TcpProxy](%s) LAST_ACK confirm fail, queue number mismatched, seq %u:%u, ack %u:%u, close fail.\n", toString().c_str(), seq, clientSeq, ack, serverSeq);
            close("close by last ack fail");
        }
    }

    bool connect()
    {
        if(dataList.size() > 0)
        {
            DataList *data = dataList[0];
            int res = destSocket.socketSend(data->data, data->size);
            if(res == 0)
            {
                close("server disconnected");
                return true;
            }
            else if(res == data->size)
            {
                connected = true;
                dataList.erase(dataList.begin());
                delete data;
                //数据即可发送给服务器
                for (int i = 0; i < dataList.size(); i++)
                {
                    DataList *data2 = dataList[0];
                    res = destSocket.socketSend(data2->data, data2->size);
                    if(res == 0)
                    {
                        close("server disconnected");
                        return true;
                    }
					else if(res == data2->size)
                    {
                    	dataList.erase(dataList.begin());
                    	delete data2;
                    	i--;
					}
					else if(res != -1)
					{
						close("connection init error");
						printf("[TcpProxy](%s) connection init error.\n", toString().c_str());
                        return true;
					}

                }
            }else if(res != -1)
            {
            	close("connection init error");
            	printf("[TcpProxy](%s) connection init error.\n", toString().c_str());
                return true;
			}
        }
        if((std::time(NULL) - createTime) > Config::TCP_CONNECT_TIMEOUT)
        {
            // printf("[TcpProxy](%s) connection init timeout, socket errorno %d.\n", toString().c_str(), errno);
            close("connection init timeout");
            return true;
        }
        return false;
    }

    bool loop()
    {
        if(state == CLOSE_WAIT || state == LAST_ACK || state == CLOSED || closed)
        {
            destSocket.iClose();
            return true;
        }

        // 与服务器未建立连接
        if(!connected)
        {
            return connect();
        }

        int len = Config::MUTE - Proxy::TCP_HEADER_SIZE;
        len = clientWindow < len ? clientWindow : len;
        if(len <= 0)
        {
            return false;
        }

        // 从服务器接收数据
        int size = destSocket.socketRecv(buffer + Proxy::TCP_HEADER_SIZE, len);
        if (size > 0)
        {
            updateTCPBuffer(buffer, TCPHeader::ACK, serverSeq, clientSeq, size);
            // 转发给客户端
            sendToClient(buffer, Proxy::TCP_HEADER_SIZE + size);
            serverSeq += size;
        }
        else if(size == 0)
        {
            close("server disconnected");
            return true;
        }
        return false;
    }

    bool equal(char *packet)
    {
        IPHeader ipHeader = IPHeader(packet, 0);
        TCPHeader tcpHeader = TCPHeader(packet, ipHeader.getHeaderLength());
        return protocol == ipHeader.getProtocol() && srcIp == ipHeader.getSourceIP() && srcPort == tcpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP() && destPort == tcpHeader.getDestinationPort();
    }

    void updateTCPBuffer(char *packet, char flag, int seq, int ack, int dataSize)
    {
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
    unsigned int serverSeq;
    // 收到处理完成客户端数据队列号
    unsigned int clientSeq;
    // IP/TCP通信状态
    int state;
    // 与服务器建立连接
    bool connected;
    static const int SYN_WAIT_ACK = 1;
    static const int ESTABLISHED = 2;
    static const int CLOSE_WAIT = 3;
    static const int LAST_ACK = 4;
    static const int CLOSED = 5;
    int myWindow;
    int clientWindow;
    std::vector<DataList *> dataList;
    IPHeader mIpHeader;
    TCPHeader mTcpHeader;
};

#endif

