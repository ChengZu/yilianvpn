#ifndef CONFIG_H
#define CONFIG_H

class Config
{
public:
	// 服务器监听端口 
	static int PORT;
	// 数据包最大值 
	static const int MUTE = 1500;
	// 用户信息头大小 
	static const int USER_INFO_HEADER_SIZE = 20;
	// 可连接客户端最大值 
	static const int MAX_CLIENT_NUM= 10;
	// 单个客户端代理最大值 
	static const int CLIENT_MAX_PROXY= 256;
	// 客户端过期时间秒 
	static const long CLIENT_EXPIRE_TIME= 60 * 60;
	// 代理过期时间秒 
	static const long PROXY_EXPIRE_TIME= 60 * 60;
	// TCP连接超时时间秒 
	static const long TCP_CONNECT_TIMEOUT= 10;
	// 用户名 
	static const int USER_NAME= 12345678;
	// 用户密码 
	static const int USER_PASSWD= 87654321;
		
};

int Config::PORT = 80;

#endif
