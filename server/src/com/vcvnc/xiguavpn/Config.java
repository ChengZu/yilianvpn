package com.vcvnc.xiguavpn;
/*
 * 程序设置参数
 */

public class Config {
	//服务器监听端口
	public static int PORT = 80;
	//数据包最大值
	public static int MUTE = 1500;
	//可连接客户端最大值
	public static int MAX_CLIENT_NUM= 10;
	//单个客户端TCP代理最大值
	public static int CLIENT_MAX_TCPPROXY= 50;
	//单个客户端UDP代理最大值
	public static int CLIENT_MAX_UDPPROXY= 50;
	//代理过期时间ms
	public static long PROXY_EXPIRE_TIME= 1000 * 60 * 60 * 12;
}
