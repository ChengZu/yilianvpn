package com.vcvnc.xiguavpn;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;

import java.net.SocketException;

public class UdpProxy extends Proxy implements Runnable {
	//连接目标服务器UDP套接字
	private DatagramSocket datagramSocket;
	//接收目标服务器数据线程
	private Thread thread = null;

	public static final int HEADER_SIZE = IPHeader.IP4_HEADER_SIZE + UDPHeader.UDP_HEADER_SIZE;

	public UdpProxy(Client client, byte[] packet) {
		super(client, packet);
		headerBytes = new byte[UdpProxy.HEADER_SIZE];
		IPHeader ipHeader = new IPHeader(headerBytes, 0);
		UDPHeader udpHeader = new UDPHeader(headerBytes, IPHeader.IP4_HEADER_SIZE);
		
		ipHeader.setHeaderLength(IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTos((byte) 0x8); // 最大吞吐量 
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
		if(thread != null)
			thread.interrupt();
		if(datagramSocket != null)
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
		sendToServer(packet);
	}

	/*
	 * 发送数据给目标服务器
	 */
	public void sendToServer(byte[] packet) {
		lastClientRefreshTime = System.currentTimeMillis();
		IPHeader ipHeader = new IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		int dataSize = ipHeader.getTotalLength() - ipHeaderLen - UDPHeader.UDP_HEADER_SIZE;

		byte[] data = new byte[dataSize];
		System.arraycopy(packet, UdpProxy.HEADER_SIZE, data, 0, dataSize);
		DatagramPacket clientPacket = new DatagramPacket(data, dataSize, CommonMethods.ipIntToInet4Address(destIp & 0xFFFFFFFF), destPort & 0xFFFF);
		
		try {
			datagramSocket.send(clientPacket);
		} catch (IOException e) {
			close();
			//e.printStackTrace();
		}
	}

	@Override
	public void run() {
		boolean noError = true;
		//最大接收缓存
		byte[] revBuf = new byte[Config.MUTE - HEADER_SIZE];
		DatagramPacket revPacket = new DatagramPacket(revBuf, revBuf.length);
		int size = 0;
		while (size != -1 && !closed && noError) {
			try {
				datagramSocket.receive(revPacket);
				size = revPacket.getLength();
				synchronized (this) {
					if (size > 0) {
						byte[] packet = new byte[HEADER_SIZE + size];
						CommonMethods.arraycopy(headerBytes, 0, packet, 0, HEADER_SIZE);
						CommonMethods.arraycopy(revPacket.getData(), 0, packet, HEADER_SIZE, size);
						updateUDPBuffer(packet, size);
						// 将数据包发送给客户端
						noError = client.sendToClient(packet, 0, HEADER_SIZE + size);
					}
					lastServerRefreshTime = System.currentTimeMillis();
				}
			} catch (IOException e) {
				noError = false;
				//e.printStackTrace();
			}
		}
		close();
	}
	
	@Override
	public boolean equal(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		UDPHeader udpHeader = new UDPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == udpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP() && destPort == udpHeader.getDestinationPort();
	}
	
	public void updateUDPBuffer(byte[] packet, int size){
		IPHeader ipHeader = new IPHeader(packet, 0);
		UDPHeader udpHeader = new UDPHeader(packet, IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTotalLength((short) (UdpProxy.HEADER_SIZE + size));
		udpHeader.setTotalLength((short) (UDPHeader.UDP_HEADER_SIZE + size));
		udpHeader.ComputeUDPChecksum(ipHeader);
	}
	
	public String toString(){
    	String str = "[" + client.id + ":" + id + "][" + srcIp + ":" + srcPort + "]->[" + destIp + ":" + destPort +"]";
    	return str;
	}
}
