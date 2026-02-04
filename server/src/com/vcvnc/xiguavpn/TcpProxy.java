package com.vcvnc.xiguavpn;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class TcpProxy extends Proxy implements Runnable {
	// 服务器数据队列号
	private int serverSeq = 0;
	// 客户端数据队列号
	private int clientSeq = 0;
	private int identification = 0;
	private int state = 0;
	private final int SYN_WAIT_ACK = 1;
	private final int ESTABLISHED = 2;
	private final int CLOSE_WAIT = 3;
	private final int LAST_ACK = 4;
	private final int CLOSED = 5;
	// 与目标服务器的套接字
	private Socket socket;
	private InputStream inputStream;
	private OutputStream outputStream;
	private boolean connected = false;
	// 初始化连接线程
	private Thread thread1;
	// 接收目标服务器数据线程
	private Thread thread2;

	private int myWindow = 65535;
	private int clientWindow = 65535;
	// 未建立连接客户端发来的数据
	public List<byte[]> clientBuffer = Collections.synchronizedList(new ArrayList<byte[]>());
	
	IPHeader ipHeader = null;
	TCPHeader tcpHeader = null;

	public static final int HEADER_SIZE = IPHeader.IP4_HEADER_SIZE + TCPHeader.TCP_HEADER_SIZE;

	public TcpProxy(Client client, byte[] packet) {
		super(client, packet, IPHeader.TCP);
		headerBytes = new byte[TcpProxy.HEADER_SIZE];
		IPHeader ipHeader = new IPHeader(headerBytes, 0);
		TCPHeader tcpHeader = new TCPHeader(headerBytes, IPHeader.IP4_HEADER_SIZE);

		ipHeader.setHeaderLength(IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTos((byte) 0x0); // 一般服务
		ipHeader.setIdentification((short) 0);
		ipHeader.setFlagsAndOffset((short) 0x4000); // 不要分片
		ipHeader.setTTL((byte) 32);
		ipHeader.setProtocol((byte) IPHeader.TCP);
		ipHeader.setSourceIP(destIp);
		ipHeader.setDestinationIP(srcIp);

		tcpHeader.setDestinationPort(srcPort);
		tcpHeader.setSourcePort(destPort);
		tcpHeader.setHeaderLength((byte) TCPHeader.TCP_HEADER_SIZE);
		tcpHeader.setFlag((byte) 0);
		tcpHeader.setWindow((short) 65535);
		tcpHeader.setUrp((short) 0);

		state = 0;
		connected = false;
	}

	public boolean sendToServer(byte[] packet, int size) {
		boolean ret = true;
		if (connected) {
			try {
				outputStream.write(packet, 0, size);
			} catch (IOException e) {
				close();
				ret = false;
				//e.printStackTrace();
			}
		} else {
			byte[] data = new byte[size];
			System.arraycopy(packet, 0, data, 0, size);
			clientBuffer.add(data);
		}
		lastClientRefreshTime = System.currentTimeMillis();
		return ret;
	}

	public boolean sendToClient(byte[] packet, int size) {
		boolean ret = client.sendToClient(packet, 0, size);
		if (!ret) {
			close();
		}
		lastClientRefreshTime = System.currentTimeMillis();
		return ret;
	}

	@Override
	public boolean isClosed() {
		return state == CLOSED;
	}

	@Override
	public void close() {
		state = CLOSED;
		if (thread1 != null)
			thread1.interrupt();
		if (thread2 != null)
			thread2.interrupt();
		try {
			if (socket != null)
				socket.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		

	}
	
	private void connect() {
		try {
			socket = new Socket(CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			inputStream = socket.getInputStream();
			outputStream = socket.getOutputStream();
			// 立刻发送缓存数据
			for (int i = 0; i < clientBuffer.size(); i++) {
				try {
					outputStream.write(clientBuffer.get(i), 0, clientBuffer.get(i).length);
				} catch (IOException e) {
					e.printStackTrace();
					close();
					break;
				}
			}
			clientBuffer.clear();
			connected = true;

			thread2 = new Thread(this, "TcpProxy(" + id + ") recv server data thread");
			thread2.start();
		} catch (UnknownHostException e) {
			System.out.printf("[%s]TcpProxy(%d) connect bad address %s:%d.\n", CommonMethods.formatTime(), id,
					CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			errorClose();
		} catch (IOException e) {
			System.out.printf("[%s]TcpProxy(%d) connect error, address %s:%d.\n", CommonMethods.formatTime(), id,
					CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			errorClose();
		}
	}

	@Override
	public void processFisrtPacket(byte[] packet, int size) {
		ipHeader = new IPHeader(packet, 0);
		tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int flags = tcpHeader.getFlag();
		// 第一个TCP包不是SYN, 销毁该代理
		if ((flags & TCPHeader.SYN) == TCPHeader.SYN) {
			thread1 = new Thread() {
				@Override
				public void run() {
					connect();
				}
			};
			thread1.setName("TcpProxy(" + id + ") connect server thread");
			thread1.start();
			processSYNPacket(packet);
		} else {
			errorClose();
		}
	}

	@Override
	public void processPacket(byte[] packet, int size) {
		ipHeader = new IPHeader(packet, 0);
		tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		clientWindow = tcpHeader.getWindow() & 0xFFFF;
		int flags = tcpHeader.getFlag();

		if ((flags | TCPHeader.SYN) == TCPHeader.SYN) {
			processSYNPacket(packet);
		} else if (flags == (TCPHeader.FIN | TCPHeader.ACK)) {
			processFINPacket(packet);
		} else if ((flags | TCPHeader.ACK) == TCPHeader.ACK) {
			processACKPacket(packet);
		} else if (flags == (TCPHeader.ACK | TCPHeader.RST)) {
			processACKPacket(packet);
			processRSTPacket(packet);
		} else if (flags == (TCPHeader.ACK | TCPHeader.PSH)) {
			processACKPacket(packet);
		} else if (flags == (TCPHeader.ACK | TCPHeader.PSH | TCPHeader.FIN)) {
			processACKPacket(packet);
			processFINPacket(packet);
		} else if ((flags | TCPHeader.RST) == TCPHeader.RST) {
			processRSTPacket(packet);
		} else if ((flags | TCPHeader.PSH) == TCPHeader.PSH) {
			processPSHPacket(packet);
		} else if ((flags | TCPHeader.URG) == TCPHeader.URG) {
			processURGPacket(packet);
		} else {
			errorClose();
			System.out.printf("[%s]Client(%d) tcpProxy(%d) not deal flags %d{%s}.\n", CommonMethods.formatTime(),
					client.id, id, flags, tcpHeader.toString());
		}
	}

	public void processSYNPacket(byte[] packet) {
		clientSeq = tcpHeader.getSeqID() + 1;
		updateTCPBuffer(headerBytes, (byte) (TCPHeader.SYN | TCPHeader.ACK), serverSeq, clientSeq, 0);
		sendToClient(headerBytes, HEADER_SIZE);
		serverSeq += 1;
		state = SYN_WAIT_ACK;
	}
	
	public void processACKPacket(byte[] packet) {
		switch (state) {
		case SYN_WAIT_ACK:
			processSYNWAITACKPacket(packet);
			break;
		case ESTABLISHED:
			processESTABLISHEDACKPacket(packet);
			break;
		case CLOSE_WAIT:
			processCLOSEWAITACKPacket(packet);
			break;
		case LAST_ACK:
			processLASTACKPacket(packet);
			break;
		case CLOSED:
			System.out.printf("[%s]TcpProxy(%d) Connect close, state=%d.\n", CommonMethods.formatTime(), id, state);
			break;
		default:
			System.out.printf("[%s]TcpProxy(%d) No function deal state=%d ACK packet.\n", CommonMethods.formatTime(), id, state);
			break;
		}
	}
	
	public void processSYNWAITACKPacket(byte[] packet) {
		if (tcpHeader.getSeqID() == clientSeq && tcpHeader.getAckID() == serverSeq) {
			state = ESTABLISHED;
		} else {
			errorClose();
			System.out.printf("[%s]TcpProxy(%d) SYN_WAIT_ACK error, ack(%d:%d), seq(%d:%d).\n", CommonMethods.formatTime(),
					id, tcpHeader.getSeqID(), clientSeq, tcpHeader.getAckID(), serverSeq);
		}
	}

	public void processESTABLISHEDACKPacket(byte[] packet) {
		int seq = tcpHeader.getSeqID();
		if (seq == clientSeq) {
			// 序列号匹配
			int totalLength = ipHeader.getTotalLength();
			int headerLen = ipHeader.getHeaderLength() + tcpHeader.getHeaderLength();
			int dataSize = totalLength - headerLen;

			if (dataSize > 0) {
				byte[] data = new byte[dataSize];
				System.arraycopy(packet, headerLen, data, 0, dataSize);
				sendToServer(data, dataSize);
				// 计算下一个序列号
				clientSeq += dataSize;
			}
			updateTCPBuffer(headerBytes, (byte) TCPHeader.ACK, serverSeq, clientSeq, 0);
			// 发送ACK回复帧
			sendToClient(headerBytes, HEADER_SIZE);

		} else if (seq < clientSeq) {
			int dataSize = ipHeader.getTotalLength() - ipHeader.getHeaderLength() - tcpHeader.getHeaderLength();
			int nextSeq = seq + dataSize;
			if (nextSeq > clientSeq) {
				errorClose();
				System.out.printf("[%s]TcpProxy(%d) ACK seq %d max %d, more new packets.\n", CommonMethods.formatTime(),
						id, nextSeq, clientSeq);
			} else if (nextSeq < clientSeq) {
				// System.out.printf("TcpProxy(%d) ACK seq %d min %d, repeat packets.\n", id,
				// nextSeq, clientSeq);
			}
		} else {
			errorClose();
			System.out.printf("[%s]TcpProxy(%d) ACK seq %d max %d, miss packets.\n", CommonMethods.formatTime(), id,
					seq, clientSeq);
		}
	}

	public void processRSTPacket(byte[] packet) {
		
	}

	public void processURGPacket(byte[] packet) {
		
	}

	public void processPSHPacket(byte[] packet) {
		
	}

	public void processFINPacket(byte[] packet) {
		clientSeq++;
		updateTCPBuffer(headerBytes, (byte) TCPHeader.ACK, serverSeq, clientSeq, 0);
		sendToClient(headerBytes, TcpProxy.HEADER_SIZE);
		state = CLOSE_WAIT;
		processCLOSEWAITACKPacket(packet);
	}

	public void processCLOSEWAITACKPacket(byte[] packet) {
		updateTCPBuffer(headerBytes, (byte) (TCPHeader.FIN | TCPHeader.ACK), serverSeq, clientSeq, 0);
		sendToClient(headerBytes, TcpProxy.HEADER_SIZE);
		state = LAST_ACK;
	}

	public void processLASTACKPacket(byte[] packet) {
		serverSeq++;
		int ack = tcpHeader.getAckID();
		int seq = tcpHeader.getSeqID();
		if (ack == serverSeq && seq == clientSeq) {
			close();
			//System.out.printf("[%s]TcpProxy(%d) LAST_ACK succeed, seq(%d:%d),ack(%d:%d).\n", CommonMethods.formatTime(), id, seq, clientSeq, ack, serverSeq);
		} else {
			errorClose();
			System.out.printf("[%s]TcpProxy(%d) LAST_ACK failed, seq(%d:%d), ack(%d:%d).\n", CommonMethods.formatTime(), id, seq, clientSeq, ack, serverSeq);
		}
	}

	// 从服务器接收数据
	@Override
	public void run() {
		boolean noError = true;
		int size = 0;
		while (size != -1 && state != CLOSED && !closed && noError) {
			try {
				int len = Config.MUTE - HEADER_SIZE;
				len = clientWindow < len ? clientWindow : len;
				if (len <= 0) {
					continue;
				}
				// 接收数据缓存
				byte[] bytes = new byte[len + HEADER_SIZE];
				size = inputStream.read(bytes, HEADER_SIZE, len);

				if (size > 0) {
					System.arraycopy(headerBytes, 0, bytes, 0, HEADER_SIZE);
					byte flag = TCPHeader.ACK;
					updateTCPBuffer(bytes, flag, serverSeq, clientSeq, size);
					// 更新已发送数据队列号
					serverSeq += size;
					// 往客户端写ACK数据包
					synchronized (this) {
						noError = sendToClient(bytes, size + HEADER_SIZE);
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
	
	public void errorClose() {
		updateTCPBuffer(headerBytes, (byte) TCPHeader.RST, 0, 0, 0);
		sendToClient(headerBytes, TcpProxy.HEADER_SIZE);
		close();
	}

	@Override
	public boolean equal(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		return protocol == ipHeader.getProtocol() && srcIp == ipHeader.getSourceIP()
				&& srcPort == tcpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP()
				&& destPort == tcpHeader.getDestinationPort();
	}

	public void updateTCPBuffer(byte[] packet, byte flag, int seq, int ack, int dataSize) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTotalLength((short) (TcpProxy.HEADER_SIZE + dataSize));
		identification++;
		ipHeader.setIdentification((short) identification);
		tcpHeader.setFlag(flag);
		tcpHeader.setSeqID(seq);
		tcpHeader.setAckID(ack);
		tcpHeader.setWindow((short) myWindow);
		tcpHeader.ComputeTCPChecksum(ipHeader);
	}
}