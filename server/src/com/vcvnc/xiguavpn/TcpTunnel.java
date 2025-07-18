package com.vcvnc.xiguavpn;

import java.util.ArrayList;

public class TcpTunnel {
	// 客户端
	private Client client = null;
	// 线程退出符号
	private boolean closed = false;
	// TCP代理数组
	public ArrayList<TcpProxy> tcpProxys = new ArrayList<TcpProxy>();

	public TcpTunnel(Client client) {
		this.client = client;
	}
	
	/*
	 * 关闭通道
	 */
	public void close() {
		closed = true;
		for (TcpProxy tcpProxy : tcpProxys) {
			tcpProxy.close();
		}
		tcpProxys.clear();
	}

	public boolean isClose() {
		return closed;
	}

	/*
	 * 接收数据包 建立新代理
	 */
	public void processPacket(byte[] packet, int size)  {
		// 清除已关闭代理，防止数据发送给已关闭代理
		clearCloseProxy();
		if (tcpProxys.size() > Config.CLIENT_MAX_TCPPROXY) {
			int clearNum =  clearExpireProxy();
			if(clearNum > 0) System.out.printf("Client(%d) tcpProxys size: %d max, clean proxy num: %d.\n", client.id, tcpProxys.size(), clearNum);
		}
		
		TcpProxy tcpProxy = null;
		//检查该代理是否创建
		for (int i = 0; i < tcpProxys.size(); i++) {
			if (tcpProxys.get(i).equal(packet)) {
				tcpProxy = tcpProxys.get(i);
				break;
			}
		}
		
		// 代理没创建，建立新代理
		if(tcpProxy == null) {
			tcpProxy = new TcpProxy(client, packet);
			tcpProxy.processPacket(this, packet, true);
		}else {
			tcpProxy.processPacket(this, packet, false);
		}
	}

	/*
	 * 清除已关闭代理
	 */
	public int clearCloseProxy() {
		int ret = 0;
		for (int i = 0; i < tcpProxys.size(); i++) {
			if (tcpProxys.get(i).isClose()) {
				tcpProxys.remove(i);
				i--;
				ret++;
			}
		}
		return ret;
	}
	
	/*
	 * 清除不活动代理
	 */
	public int clearExpireProxy() {
		int ret = 0;
		for (int i = 0; i < tcpProxys.size(); i++) {
			if (tcpProxys.get(i).isExpire()) {
				tcpProxys.get(i).close();
				tcpProxys.remove(i);
				i--;
				ret++;
			}
		}
		return ret;
	}
	
	/*
	 * 清除所有代理
	 */
	public void clearAllProxy() {
		for (int i = 0; i < tcpProxys.size(); i++) {
			tcpProxys.get(i).close();
		}
		tcpProxys.clear();
	}
}