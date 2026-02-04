package com.vcvnc.xiguavpn;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;

public class Client implements Runnable {
	// 客户端套接字
	private Socket socket;
	// 客户端套接字输入流
	private InputStream inputStream;
	// 客户端套接字输出流
	private OutputStream outputStream;
	// 缓存数据
	private byte[] cacheBytes;
	// 是否有缓存数据
	private boolean haveCacheBytes;
	// TCP数据处理
	private ProxyContainer proxyContainer = null;

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
		id = UID;
		this.socket = socket;
		proxyContainer = new ProxyContainer(this);
		try {
			inputStream = socket.getInputStream();
			outputStream = socket.getOutputStream();
			thread = new Thread(this, "Client(" + id + ")");
			thread.start();
		} catch (IOException e) {
			e.printStackTrace();
			close();
		}
	}

	/*
	 * 关闭客户端 关闭客户端所有代理
	 */
	public void close() {
		closed = true;
		if (thread != null)
			thread.interrupt();
		proxyContainer.clearAllProxy();
		try {
			socket.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		System.out.printf("[%s]Client(%d) closed.\n", CommonMethods.formatTime(), id);
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
			outputStream.write(packet, offset, size);
		} catch (IOException e) {
			//e.printStackTrace();
			close();
			return false;
		}
		return true;
	}

	/*
	 * 处理IP数据包 TCP包让tcpTunnel处理 UDP包让udpTunnel处理 其他包不处理，并关闭客户端
	 */
	private int processIPPacket(byte[] bytes, int size) {
		int ret = 0;
		IPHeader header = new IPHeader(bytes, 0);
		byte protocol = header.getProtocol();
		if (protocol == IPHeader.TCP) {
			proxyContainer.processPacket(bytes, size, IPHeader.TCP);
		} else if (protocol == IPHeader.UDP) {
			proxyContainer.processPacket(bytes, size, IPHeader.UDP);
		} else {
			System.out.printf("[%s]Client(%d) recvive unknown protocol packet.\n", CommonMethods.formatTime(), id);
			ret = -1;
		}
		return ret;
	}

	/*
	 * 对接收的数据分包
	 */
	private int processRecvPacket(byte[] bytes, int size) {
		int ret = 0;
		if (haveCacheBytes) {
			byte[] data = new byte[cacheBytes.length + size];
			System.arraycopy(cacheBytes, 0, data, 0, cacheBytes.length);
			System.arraycopy(bytes, 0, data, cacheBytes.length, size);
			size = cacheBytes.length + size;
			haveCacheBytes = false;
			ret = processRecvPacket(data, size);
			return ret;
		}

		if (size < IPHeader.IP4_HEADER_SIZE) {
			byte[] data = new byte[size];
			System.arraycopy(bytes, 0, data, 0, size);
			cacheBytes = data;
			haveCacheBytes = true;
			return 0;
		}

		IPHeader IpHeader = new IPHeader(bytes, 0);
		int totalLength = IpHeader.getTotalLength();
		if (totalLength < 0 || totalLength > Config.MUTE || size > (Config.MUTE * 2)) { // 长度非法
			System.out.printf("[%s]Client(%d) recvive bad length packet.\n", CommonMethods.formatTime(), id);
			return -2;
		}

		if (size > totalLength) {
			ret = processIPPacket(bytes, totalLength);
			int nextDataSize = size - totalLength;
			byte[] data = new byte[nextDataSize];
			System.arraycopy(bytes, totalLength, data, 0, nextDataSize);
			ret = processRecvPacket(data, nextDataSize);
		} else if (size == totalLength) {
			ret = processIPPacket(bytes, size);
		} else if (size < totalLength) {
			byte[] data = new byte[size];
			System.arraycopy(bytes, 0, data, 0, size);
			cacheBytes = data;
			haveCacheBytes = true;
		}
		return ret;
	}

	@Override
	public void run() {
		boolean noError = true;
		int size = 0;
		// 读取头Config.USER_INFO_HEADER_SIZE字节，验证用户名和密码
		int readSize = 0;
		byte[] readBytes = new byte[Config.USER_INFO_HEADER_SIZE];
		while (size != -1 && readSize < Config.USER_INFO_HEADER_SIZE && noError) {
			try {
				size = inputStream.read(readBytes, readSize, Config.USER_INFO_HEADER_SIZE - readSize);
			} catch (IOException e) {
				e.printStackTrace();
				noError = false;
			}
			if (size != -1) {
				readSize += size;
			}
		}

		if (readSize < Config.USER_INFO_HEADER_SIZE) {
			close();
			return;
		}

		IPHeader header = new IPHeader(readBytes, 0);
		// header.getSourceIP() 为用户名
		// header.getDestinationIP() 为密码
		if (header.getSourceIP() != Config.USER_NAME && header.getDestinationIP() != Config.USER_PASSWD) {
			close();
			return;
		}
		// 接收IP数据包
		while (size != -1 && !closed && noError) {
			byte[] bytes = new byte[Config.MUTE];
			try {
				size = inputStream.read(bytes);
				lastRefreshTime = System.currentTimeMillis();
			} catch (IOException e) {
				e.printStackTrace();
				noError = false;
			}
			if (size > 0) {
				int res = processRecvPacket(bytes, size);
				if (res != 0)
					noError = false;
			}
		}
		close();
	}

	public boolean isExpire() {
		long now = System.currentTimeMillis();
		return (now - lastRefreshTime) > Config.CLIENT_EXPIRE_TIME;
	}
}
