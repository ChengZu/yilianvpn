#ifndef VPNSERVER_H
#define VPNSERVER_H
#include <vector>
#include "Loop.h"
#include "Client.h"
#include "Socket.h"
#include "ServerSocket.h"
#include "Config.h"

class VpnServer: public Task
{
public:
	VpnServer(){
		closed = false;
		int res = server.init(0, Config::PORT);
		if(res == -1){
			perror("[VpnServer]套接字错误消息");
			printf("[VpnServer]服务器套接字绑定0.0.0.0:%d失败。\n", Config::PORT);
			close();
		}else{
			printf("[VpnServer]服务器套接字已绑定0.0.0.0:%d。\n", Config::PORT);
			iloop = Loop(this, (char*)"VpnServer");
			iloop.join();
		}
	}
	
	~VpnServer(){
		close();
	}
	
	void close(){
		closed = true;
		iloop.quit();
		server.iClose();
		for(int i=0; i<clients.size(); i++){
			Client* client = clients[i];
			delete client;
		}
	}
	
	bool isClose(){
		return closed;
	}
	
	bool loop(){
		// 移除已关闭客户端 
		for(int i=0; i<clients.size(); i++){
			Client* client = clients[i];
			if(client->isClose() || client->isExpire()){
				clients.erase(clients.begin() + i);
				i--;
				delete client;
			}
		}
		int socket_fd = server.getClientSocket();
		if(socket_fd == -1){
			return false;
		}else if(socket_fd == 0){
			printf("[VpnServer]错误：服务器套接字已关闭。\n");
			close();
			return true;
		}
		Socket* socket = new Socket(socket_fd);
		// 是否建立客户端 
		if (clients.size() < Config::MAX_CLIENT_NUM) {
			Client* client = new Client(socket);
			clients.push_back(client);
			printf("[VpnServer]新客户端(%ld)正在建立连接，总客户端数量：%ld。\n", client->myId, clients.size());
		} else {
			socket->iClose();
			printf("[VpnServer]客户端连接数量达到最大，共：%ld, 关闭新客户端连接。\n", clients.size());
		}
		return false;
	}

private:
	// 服务器套接字 
	ServerSocket server;
	// 客户端容器 
	std::vector<Client*> clients;
	// 已关闭状态 
	bool closed;
	// 接收新客户端任务 
	Loop iloop;
};

#endif
