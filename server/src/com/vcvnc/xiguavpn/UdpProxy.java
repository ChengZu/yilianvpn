package com.vcvnc.xiguavpn;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;

import java.net.SocketException;

public class UdpProxy extends Proxy implements Runnable {
	// 连接目标服务器UDP套接字
	private DatagramSocket datagramSocket;
	// 接收目标服务器数据线程
	private Thread thread = null;

	public static final int HEADER_SIZE = IPHeader.IP4_HEADER_SIZE + UDPHeader.UDP_HEADER_SIZE;

	public UdpProxy(Client client, byte[] packet) {
		super(client, packet, IPHeader.UDP);
		headerBytes = new byte[UdpProxy.HEADER_SIZE];
		IPHeader ipHeader = new IPHeader(headerBytes, 0);
		UDPHeader udpHeader = new UDPHeader(headerBytes, IPHeader.IP4_HEADER_SIZE);

		ipHeader.setHeaderLength(IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTos((byte) 0x0); // 一般服务
		ipHeader.setIdentification((short) 0);
		ipHeader.setFlagsAndOffset((short) 0x4000); // 不要分片
		ipHeader.setTTL((byte) 32);
		ipHeader.setProtocol((byte) IPHeader.UDP);
		ipHeader.setSourceIP(destIp);
		ipHeader.setDestinationIP(srcIp);

		udpHeader.setSourcePort(destPort);
		udpHeader.setDestinationPort(srcPort);
	}

	@Override
	public void close() {
		closed = true;
		if (thread != null)
			thread.interrupt();
		if (datagramSocket != null)
			datagramSocket.close();
	}

	@Override
	public boolean isClosed() {
		return closed;
	}

	@Override
	public void processFisrtPacket(byte[] packet, int size) {
		try {
			datagramSocket = new DatagramSocket();
			thread = new Thread(this, "UdpProxy(" + id + ")");
			thread.start();
		} catch (SocketException e) {
			close();
			e.printStackTrace();
		}
		processPacket(packet, size);
	}

	@Override
	public void processPacket(byte[] packet, int size) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		int dataSize = ipHeader.getTotalLength() - ipHeaderLen - UDPHeader.UDP_HEADER_SIZE;

		byte[] data = new byte[dataSize];
		System.arraycopy(packet, UdpProxy.HEADER_SIZE, data, 0, dataSize);
		DatagramPacket clientPacket = new DatagramPacket(data, dataSize,
				CommonMethods.ipIntToInet4Address(destIp & 0xFFFFFFFF), destPort & 0xFFFF);

		try {
			// 发送数据给目标服务器
			datagramSocket.send(clientPacket);
		} catch (IOException e) {
			close();
			e.printStackTrace();
		}
		lastClientRefreshTime = System.currentTimeMillis();
	}

	@Override
	public void run() {
		boolean noError = true;
		// 最大接收缓存
		byte[] revBuf = new byte[Config.MUTE - HEADER_SIZE];
		// 服务器数据缓存
		byte[] packet = new byte[Config.MUTE];
		DatagramPacket revPacket = new DatagramPacket(revBuf, revBuf.length);
		int size = 0;
		while (size != -1 && !closed && noError) {
			try {
				datagramSocket.receive(revPacket);
				size = revPacket.getLength();
				if (size > 0) {
					int packetLen = HEADER_SIZE + size;
					// byte[] packet = new byte[packetLen];
					CommonMethods.arraycopy(headerBytes, 0, packet, 0, HEADER_SIZE);
					CommonMethods.arraycopy(revPacket.getData(), 0, packet, HEADER_SIZE, size);
					updateUDPBuffer(packet, size);
					synchronized (this) {
						// 将数据包发送给客户端
						noError = client.sendToClient(packet, 0, packetLen);
					}
				}
				lastServerRefreshTime = System.currentTimeMillis();
			} catch (IOException e) {
				noError = false;
				// e.printStackTrace();
			}
		}
		close();
	}

	@Override
	public boolean equal(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		UDPHeader udpHeader = new UDPHeader(packet, ipHeader.getHeaderLength());
		return protocol == ipHeader.getProtocol() && srcIp == ipHeader.getSourceIP()
				&& srcPort == udpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP()
				&& destPort == udpHeader.getDestinationPort();
	}

	public void updateUDPBuffer(byte[] packet, int size) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		UDPHeader udpHeader = new UDPHeader(packet, IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTotalLength((short) (UdpProxy.HEADER_SIZE + size));
		udpHeader.setTotalLength((short) (UDPHeader.UDP_HEADER_SIZE + size));
		udpHeader.ComputeUDPChecksum(ipHeader);
	}

	public String toString() {
		String str = "[" + client.id + ":" + id + "][" + srcIp + ":" + srcPort + "]->[" + destIp + ":" + destPort + "]";
		return str;
	}
}
