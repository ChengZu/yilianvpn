#ifndef VPNSERVER_H
#define VPNSERVER_H
#include <vector>
#include "Client.h"
#include "Socket.h"
#include "ServerSocket.h"
#include "Config.h"

class VpnServer: public Task
{
public:
    VpnServer()
    {
        name = "VpnServer";
        closed = false;
        int res = server.init(0, Config::PORT);
        if(res == -1)
        {
            perror("[VpnServer]socket error msg");
            printf("[VpnServer]socket bind 0.0.0.0:%d fail.\n", Config::PORT);
            close();
        }
        else
        {
            printf("[VpnServer]socket bind 0.0.0.0:%d success.\n", Config::PORT);
        }
    }

    ~VpnServer()
    {
    }

    void close()
    {
        quit();
        closed = true;
        server.iClose();
        closeAllClient();
    }

    bool isClose()
    {
        return closed;
    }

    int closeAllClient()
    {
        int ret = 0;
        for (int i = 0; i < clients.size(); i++)
        {
            Client *client = clients[i];
            client->close();
            clients.erase(clients.begin());
            i--;
            ret++;
            delete client;
        }
        return ret;
    }

    bool loop()
    {
        // 移除已关闭客户端
        for(int i = 0; i < clients.size(); i++)
        {
            Client *client = clients[i];
            bool needRemove = false;
            if(client->isClose())
            {
                needRemove = true;
            }
            if(client->isExpire())
            {
                client->close();
                needRemove = true;

            }
            if(needRemove)
            {
                clients.erase(clients.begin() + i);
                i--;
                delete client;
            }
        }
        int socket_fd = server.getClientSocket();
        if(socket_fd == -1)
        {
            return false;
        }
        if(socket_fd == 0)
        {
            close();
            perror("[VpnServer]socket error msg");
            printf("[VpnServer]vpnserver socket error closed.\n");
            return true;
        }
        Socket socket = Socket(socket_fd);
        // 是否建立客户端
        if (clients.size() < Config::MAX_CLIENT_NUM)
        {
            Client *client = new Client(socket);
            clients.push_back(client);
            printf("[VpnServer]new client(%ld)connecting, total client number %ld.\n", client->clientId, clients.size());
        }
        else
        {
            socket.iClose();
            printf("[VpnServer]client connet number reach max, total: %ld, client closeing.\n", clients.size());
        }
        return false;
    }

private:
    // 服务器套接字
    ServerSocket server;
    // 客户端容器
    std::vector<Client *> clients;
    // 已关闭状态
    bool closed;
};

#endif
