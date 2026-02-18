package com.vcvnc.xiguavpn;

public abstract class Proxy {
	// 客户端套接字 
	public Client client;
	// 保存UDP连接信息, 头部信息 
	public byte[] headerBytes;
	// 已关闭状态 
	public boolean closed;
	// ip协议
	public char protocol = (char) -1;
	// 源ip 
	public int srcIp;
	// 源端口 
	public short srcPort;
	// 目标ip 
	public int destIp;
	// 目标端口 
	public short destPort; 
	// 错误消息 
	public String  errorMsg;
	// 创建时间
	public long createTime; 
	// 客户端最后刷新时间 
	public long lastClientRefreshTime = System.currentTimeMillis();;
	// 服务器最后刷新时间 
	public long lastServerRefreshTime = System.currentTimeMillis();;
	// ID数字 
	public static long UID = 0;
	// id 
	public long id;
	
	public Proxy() {
	}
	
	public Proxy(Client client, byte[] packet, char protocol) {
		UID++;
		id = UID;
		this.client = client;
		this.protocol = protocol;
		IPHeader ipHeader = new IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		srcIp = ipHeader.getSourceIP();
		destIp = ipHeader.getDestinationIP();
		if (protocol == IPHeader.TCP) {
			TCPHeader tcpHeader = new TCPHeader(packet, ipHeaderLen);
			srcPort = tcpHeader.getSourcePort();
			destPort = tcpHeader.getDestinationPort();
		} else if (protocol == IPHeader.UDP) {
			UDPHeader udpHeader = new UDPHeader(packet, ipHeaderLen);
			srcPort = udpHeader.getSourcePort();
			destPort = udpHeader.getDestinationPort();
		}else {
			close();
		}

	}
	
	public abstract void close();
	public abstract boolean isClosed();
	public abstract void processFisrtPacket(byte[] packet, int size);
	public abstract void processPacket(byte[] packet, int size);
	public abstract boolean equal(byte[] packet);
	
	boolean isExpire(){
		long now = System.currentTimeMillis();
		return (now - lastClientRefreshTime) > Config.PROXY_EXPIRE_TIME || (now - lastServerRefreshTime) > Config.PROXY_EXPIRE_TIME;
	}
	
	public String toString(){
    	String str = "[" + client.id + ":" + id + "][" + srcIp + ":" + srcPort + "]->[" + destIp + ":" + destPort +"]";
    	return str;
	}
	
}
