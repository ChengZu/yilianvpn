package com.vcvnc.xiguavpn;

import java.util.ArrayList;

public class UdpTunnel {
	// 客户端
	private Client client = null;
	// 线程退出符号
	private boolean closed = false;
	// UDP代理数组
	public ArrayList<UdpProxy> udpProxys = new ArrayList<>();

	public UdpTunnel(Client client) {
		this.client = client;
	}

	/*
	 * 关闭通道
	 */
	public void close() {
		closed = true;
		for (UdpProxy udpProxy : udpProxys) {
			udpProxy.close();
		}
		udpProxys.clear();
	}

	public boolean isClose() {
		return closed;
	}

	/*
	 * 接收数据包 建立新代理
	 */
	public void processPacket(byte[] packet, int size){
		// 清除已关闭代理，防止数据发送给已关闭代理
		clearCloseProxy();
		if (udpProxys.size() > Config.CLIENT_MAX_UDPPROXY) {
			int clearNum = clearExpireProxy();
			if (clearNum > 0) System.out.printf("Client(%d) udpProxys size: %d max, clear proxy num: %d.\n", client.id, udpProxys.size(), clearNum);
		}

		// 检查该代理是否已创建
		UdpProxy udpProxy = null;
		for (int i = 0; i < udpProxys.size(); i++) {
			if (udpProxys.get(i).equal(packet)) {
				udpProxy = udpProxys.get(i);
				break;
			}
		}

		// 代理没创建，建立新代理
		if (udpProxy == null) {
			udpProxy = new UdpProxy(client, packet);
			udpProxys.add(udpProxy);
			udpProxy.processPacket(packet, size);
		}else {
			udpProxy.processPacket(packet, size);
		}
	}
	
	/*
	 * 清除已关闭代理
	 */
	public int clearCloseProxy() {
		int ret = 0;
		for (int i = 0; i < udpProxys.size(); i++) {
			if (udpProxys.get(i).isClose()) {
				udpProxys.remove(i);
				i--;
				ret++;
			}
		}
		return ret;
	}

	/*
	 * 清除过期代理
	 */
	public int clearExpireProxy() {
		int ret = 0;
		for (int i = 0; i < udpProxys.size(); i++) {
			if (udpProxys.get(i).isExpire()) {
				udpProxys.get(i).close();
				udpProxys.remove(i);
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
		for (int i = 0; i < udpProxys.size(); i++) {
			udpProxys.get(i).close();
		}
		udpProxys.clear();
	}
}