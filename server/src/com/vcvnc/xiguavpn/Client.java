package com.vcvnc.xiguavpn;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;

public class Client implements Runnable {
	// 客户端套接字
	private Socket socket;
	// 客户端套接字输入流
	private InputStream is;
	// 客户端套接字输出流
	private OutputStream os;
	// 缓存数据
	private byte[] cacheBytes;
	// 是否有缓存数据
	private boolean haveCacheBytes;
	// TCP数据处理通道
	private TcpTunnel tcpTunnel = null;
	// UDP数据处理通道
	private UdpTunnel udpTunnel = null;
	// 线程退出符号
	private boolean closed = false;
	// 客户端id
	public long id;
	// 客户端id生成数
	private static long UID = 0;
	// 接收客户端数据线程
	private Thread thread = null;
	
	public long lastRefreshTime = System.currentTimeMillis();

	public Client(Socket socket) {
		UID++;
		this.id = UID;
		this.socket = socket;
		tcpTunnel = new TcpTunnel(this);
		udpTunnel = new UdpTunnel(this);
		try {
			is = socket.getInputStream();
			os = socket.getOutputStream();
			thread = new Thread(this, "Client(" + id + ")");
			thread.start();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			close();
		}
	}

	/*
	 * 关闭客户端 关闭客户端所有代理
	 */
	public void close() {
		closed = true;
		if(thread != null)
			thread.interrupt();
		tcpTunnel.clearAllProxy();
		udpTunnel.clearAllProxy();
		try {
			socket.close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		System.out.printf("Client(%d) close.\n", id);
	}

	public boolean isClose() {
		return closed;
	}

	/*
	 * 把数据发送给客户端
	 */
	public synchronized boolean sendToClient(byte[] packet, int offset, int size) {
		lastRefreshTime = System.currentTimeMillis();
		if (isClose())
			return false;
		try {
			os.write(packet, offset, size);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			close();
			return false;
		}
		return true;
	}

	/*
	 * 对接收的数据分包
	 */
	private void processRecvPacket(byte[] bytes, int size) throws Exception {
		if (this.haveCacheBytes) {
			byte[] data = new byte[this.cacheBytes.length + size];
			System.arraycopy(this.cacheBytes, 0, data, 0, this.cacheBytes.length);
			System.arraycopy(bytes, 0, data, this.cacheBytes.length, size);
			bytes = data;
			size = this.cacheBytes.length + size;
			this.haveCacheBytes = false;
		}

		if (size < IPHeader.IP4_HEADER_SIZE) {
			byte[] data = new byte[size];
			System.arraycopy(bytes, 0, data, 0, size);
			this.cacheBytes = data;
			this.haveCacheBytes = true;
			return;
		}

		IPHeader IpHeader = new IPHeader(bytes, 0);
		int totalLength = IpHeader.getTotalLength();

		if (size > totalLength) {
			processIPPacket(bytes, totalLength);
			int nextDataSize = size - totalLength;
			byte[] data = new byte[nextDataSize];
			System.arraycopy(bytes, totalLength, data, 0, nextDataSize);
			processRecvPacket(data, nextDataSize);
		} else if (size == totalLength) {
			processIPPacket(bytes, size);
		} else if (size < totalLength) {
			byte[] data = new byte[size];
			System.arraycopy(bytes, 0, data, 0, size);
			this.cacheBytes = data;
			this.haveCacheBytes = true;
		}
	}

	/*
	 * 处理IP数据包 TCP包让tcpTunnel处理 UDP包让udpTunnel处理 其他包不处理，并关闭客户端
	 */
	private void processIPPacket(byte[] bytes, int size) throws Exception {
		IPHeader header = new IPHeader(bytes, 0);
		byte protocol = header.getProtocol();
		if (protocol == IPHeader.TCP) {
			tcpTunnel.processPacket(bytes, size);
		} else if (protocol == IPHeader.UDP) {
			udpTunnel.processPacket(bytes, size);
		} else {
			throw new Exception("Client(" + id + ") recvice unkonw protocol packet");
		}
	}

	@Override
	public void run() {
		boolean noError = true;
		int size = 0;
		// 读取头20字节，验证用户名密码
		int fistPacketSize = 20;
		int readSize = 0;
		byte[] readBytes = new byte[fistPacketSize];
		while (size != -1 && readSize < fistPacketSize && noError) {
			try {
				size = is.read(readBytes, readSize, fistPacketSize - readSize);
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
				noError = false;
			}
			if (size != -1) {
				readSize += size;
			}
		}

		if (readSize < fistPacketSize) {
			close();
			return;
		}

		IPHeader header = new IPHeader(readBytes, 0);
		// header.getSourceIP() 为用户名
		// header.getDestinationIP() 为密码
		if (header.getSourceIP() != 12345678 && header.getDestinationIP() != 87654321) {
			close();
			return;
		}
		// 接收IP数据包
		while (size != -1 && !closed && noError) {
			byte[] bytes = new byte[Config.MUTE];
			try {
				size = is.read(bytes);
				lastRefreshTime = System.currentTimeMillis();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				//e.printStackTrace();
				noError = false;
			}
			if (size > 0) {
				try {
					processRecvPacket(bytes, size);
				} catch (Exception e) {
					// TODO Auto-generated catch block
					//e.printStackTrace();
					noError = false;
				}
			}
		}
		close();
	}

	public boolean isExpire(){
		long now = System.currentTimeMillis();
		return (now - lastRefreshTime) > Config.PROXY_EXPIRE_TIME;
	}
}
