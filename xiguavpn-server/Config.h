#ifndef CONFIG_H
#define CONFIG_H

class Config
{
public:
	// 服务器监听端口
	static int PORT;
	// 数据包最大值
	static const int MUTE = 1500;
	// 可连接客户端最大值
	static const int MAX_CLIENT_NUM = 10;
	// 单个客户端代理最大值
	static const int CLIENT_MAX_PROXY = 512;
	// 客户端过期时间秒
	static const long CLIENT_EXPIRE_TIME = 15;
	// 代理过期时间秒
	static const long PROXY_EXPIRE_TIME = 30;
	// TCP连接超时时间秒
	static const long TCP_CONNECT_TIMEOUT = 15;
	// 用户信息头大小
	static const int USER_INFO_HEADER_SIZE = 20;
	// 用户名
	static const int USER_NAME = 12345678;
	// 用户密码
	static const int USER_PASSWD = 87654321;
};

int Config::PORT = 4430;

#endif
