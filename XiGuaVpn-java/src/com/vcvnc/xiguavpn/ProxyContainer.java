package com.vcvnc.xiguavpn;

import java.util.ArrayList;

public class ProxyContainer {
	// 客户端
	private Client client = null;
	// 线程退出符号
	private boolean closed = false;
	// TCP代理数组
	public ArrayList<Proxy> proxys = new ArrayList<Proxy>();

	public ProxyContainer(Client client) {
		this.client = client;
	}
	
	/*
	 * 关闭通道
	 */
	public void close() {
		closed = true;
		for (Proxy proxy : proxys) {
			proxy.close();
		}
		proxys.clear();
	}

	public boolean isClose() {
		return closed;
	}

	/*
	 * 接收数据包 建立新代理
	 */
	public void processPacket(byte[] packet, int size, char protocol)  {
		// 清除已关闭代理，防止数据发送给已关闭代理
		clearCloseProxy();
		if (proxys.size() > Config.CLIENT_MAX_PROXY) {
			int clearNum =  clearExpireProxy();
			if(clearNum > 0) System.out.printf("[Proxycontainer]client(%d) tcpProxys size: %d max, clean proxy num: %d.\n", client.id, proxys.size(), clearNum);
		}
		
		Proxy proxy = null;
		//检查该代理是否创建
		for (int i = 0; i < proxys.size(); i++) {
			if (proxys.get(i).equal(packet)) {
				proxy = proxys.get(i);
				break;
			}
		}
		
		// 代理没创建，建立新代理
		if(proxy == null) {
			//proxy = new TcpProxy(client, packet);
			if(protocol == IPHeader.TCP){
				proxy = new TcpProxy(client, packet);
			}else if(protocol == IPHeader.UDP){
				proxy = new UdpProxy(client, packet);
			}else{
				System.out.printf("[Proxycontainer]client(%d) create new proxy(%s) error.\n", client.id, proxy);
				return;
			}
			proxys.add(proxy);
			proxy.processFisrtPacket(packet, size);
		}else {
			proxy.processPacket(packet, size);
		}
	}

	/*
	 * 清除已关闭代理
	 */
	public int clearCloseProxy() {
		int ret = 0;
		for (int i = 0; i < proxys.size(); i++) {
			if (proxys.get(i).isClosed()) {
				proxys.remove(i);
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
		for (int i = 0; i < proxys.size(); i++) {
			if (proxys.get(i).isExpire()) {
				proxys.get(i).close();
				proxys.remove(i);
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
		for (int i = 0; i < proxys.size(); i++) {
			proxys.get(i).close();
		}
		proxys.clear();
	}
}
