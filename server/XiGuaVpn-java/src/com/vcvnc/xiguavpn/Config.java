package com.vcvnc.xiguavpn;
/*
 * 程序设置参数
 */

public class Config {
	// 服务器监听端口 
	public static int PORT = 80;
	// 数据包最大值 
	public static int MUTE = 1500;
	// 用户信息头大小 
	public static int USER_INFO_HEADER_SIZE = 20;
	// 可连接客户端最大值 
	public static int MAX_CLIENT_NUM= 10;
	// 单个客户端TCP代理最大值 
	public static int CLIENT_MAX_TCPPROXY= 256;
	// 单个客户端UDP代理最大值 
	public static int CLIENT_MAX_UDPPROXY= 256;
	// 客户端过期时间秒 
	public static long CLIENT_EXPIRE_TIME= 60 * 3;
	// TCP代理过期时间秒 
	public static long TCPPROXY_EXPIRE_TIME= 60 * 60;
	// UDP代理过期时间秒 
	public static long UDPPROXY_EXPIRE_TIME= 60 * 5;
	// TCP连接超时时间秒 
	public static long TCP_CONNECT_TIMEOUT= 10;
	// 用户名 
	public static int USER_NAME= 12345678;
	// 用户密码 
	public static int USER_PASSWD= 87654321;
}
