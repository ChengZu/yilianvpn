package com.vcvnc.xiguavpn;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.concurrent.CopyOnWriteArrayList;


public class TcpProxy implements Runnable {
	// 客户端
	private Client client;
	// 头部信息
	private byte[] headerBytes = null;
	// 源ip
	public int srcIp;
	// 源端口
	public short srcPort;
	// 目标ip
	public int destIp;
	// 目标端口
	public short destPort;
	// 发送给客户端数据队列号
	private int mySeq;
	// 收到处理完成客户端数据队列号
	private int myAck;
	private int identification = 0;
	private int state = 0;
	private final int SYN_RCVD = 1;
	private final int ESTABLISHED = 2;
	private final int CLOSE_WAIT = 3;
	private final int LAST_ACK = 4;
	private final int CLOSED = 8;
	// 与目标服务器的套接字
	private Socket socket;
	private InputStream is;
	private OutputStream os;
	// 未建立连接客户端发来的数据
	private CopyOnWriteArrayList<byte[]> waitData = new CopyOnWriteArrayList<byte[]>();
	boolean connectSucceed = false;
	public boolean closed = false;
	private static long UID = 0;
	public long id;
	// 初始化连接线程
	private Thread thread1;
	// 接收目标服务器数据线程
	private Thread thread2;
	// 最后活动时间
	public long lastRefreshTime = System.currentTimeMillis();
	public static final int HEADER_SIZE = IPHeader.IP4_HEADER_SIZE + TCPHeader.TCP_HEADER_SIZE;

	public TcpProxy(Client client, byte[] packet) {
		UID++;
		id = UID;
		this.client = client;
		IPHeader ipHeader = new IPHeader(packet, 0);
		int ipHeaderLen = ipHeader.getHeaderLength();
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeaderLen);
		srcIp = ipHeader.getSourceIP();
		srcPort = tcpHeader.getSourcePort();
		destIp = ipHeader.getDestinationIP();
		destPort = tcpHeader.getDestinationPort();
		mySeq = 12345;
		myAck = tcpHeader.getSeqID() + 1;
		
		headerBytes = new byte[TcpProxy.HEADER_SIZE];
		CommonMethods.arraycopy(packet, 0, headerBytes, 0, IPHeader.IP4_HEADER_SIZE);
		CommonMethods.arraycopy(packet, ipHeaderLen, headerBytes, IPHeader.IP4_HEADER_SIZE, TCPHeader.TCP_HEADER_SIZE);
		ipHeader = new IPHeader(headerBytes, 0);
		tcpHeader = new TCPHeader(headerBytes, IPHeader.IP4_HEADER_SIZE);
		ipHeader.setSourceIP(destIp);
		tcpHeader.setSourcePort(destPort);
		ipHeader.setDestinationIP(srcIp);
		tcpHeader.setDestinationPort(srcPort);
		ipHeader.setFlagsAndOffset((short) 0);
		tcpHeader.setWindow((short) 65535);
		tcpHeader.setUrp((short) 0);
		tcpHeader.setHeaderLength((byte) 20);

		thread1 = new Thread() {
			@Override
			public void run() {
				connect();
			}
		};
		thread1.setName("TcpProxy(" + id + ") connect server thread");
		thread1.start();
	}

	private void connect() {
		try {
			socket = new Socket(CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			is = socket.getInputStream();
			os = socket.getOutputStream();
			connectSucceed = true;
		} catch (UnknownHostException e) {
			// TODO Auto-generated catch block
			System.out.printf("TcpProxy(%d) Connect bad address %s:%d.\n", id, CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			// e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			System.out.printf("TcpProxy(%d) Connect error, address %s:%d.\n", id, CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			// e.printStackTrace();
		}
		if (connectSucceed) {
			thread2 = new Thread(this, "TcpProxy(" + id + ") recv server data thread");
			thread2.start();
			// 发送缓存的数据给目标服务器
			if (waitData.size() > 0) {
				for (int i = 0; i < waitData.size(); i++) {
					try {
						os.write(waitData.get(i), 0, waitData.get(i).length);
					} catch (IOException e) {
						// TODO Auto-generated catch block
						disConnectClient();
						//e.printStackTrace();
						break;
					}
					waitData.remove(i);
					i--;
				}
			}
		} else {
			state = CLOSED;
		}
	}

	public void writeToServer(byte[] packet, int offset, int size) {
		lastRefreshTime = System.currentTimeMillis();
		if (connectSucceed) {
			try {
				os.write(packet, offset, size);
			} catch (IOException e) {
				// TODO Auto-generated catch block
				destroy();
				e.printStackTrace();
			}
		} else {
			byte[] data = new byte[size];
			System.arraycopy(packet, offset, data, 0, size);
			waitData.add(data);
		}
	}
	
	public boolean isClose() {
		return state == CLOSED;
	}
	
	public void destroy() {
		close();
		state = CLOSED;
	}

	public void close() {
		closed = true;
		if(thread1 != null)
			thread1.interrupt();
		if(thread2 != null)
			thread2.interrupt();
		try {
			if(socket !=  null)
				socket.close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void processPacket(TcpTunnel tcpTunnel, byte[] packet, boolean newTcpProxy) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, IPHeader.IP4_HEADER_SIZE);
		int flags = tcpHeader.getFlag();
		if(newTcpProxy && (flags & TCPHeader.SYN) != TCPHeader.SYN) {
			if((flags & TCPHeader.RST) != TCPHeader.RST){
				updateTCPBuffer(headerBytes, (byte)TCPHeader.RST, mySeq, myAck, 0);
				client.sendToClient(headerBytes, 0, TcpProxy.HEADER_SIZE);
			}
			destroy();
			return;
		}
		if(newTcpProxy) {
			tcpTunnel.tcpProxys.add(this);
		}
		
		if((flags & TCPHeader.SYN) == TCPHeader.SYN){
			processSYNPacket(packet);
		}else
		
		if((flags & TCPHeader.SYN) == TCPHeader.SYN && (flags & TCPHeader.ACK) == TCPHeader.ACK){
			processACKPacket(packet);
		}else
		
		if((flags & TCPHeader.FIN) == TCPHeader.FIN && (flags & TCPHeader.ACK) == TCPHeader.ACK){
			processFINPacket(packet);
		}else
		
		if((flags & TCPHeader.ACK) == TCPHeader.ACK){
			processACKPacket(packet);
		}else
		
		if((flags & TCPHeader.RST) == TCPHeader.RST){
			processRSTPacket(packet);
		}else
		
		if((flags & TCPHeader.PSH) == TCPHeader.PSH){
			processPSHPacket(packet);
		}else
		
		if((flags & TCPHeader.URG) == TCPHeader.URG){
			processURGPacket(packet);
		}else{
			System.out.printf("Client(%ld) tcpProxy(%s:%d) 未处理标志位%d{%s}.\n", client.id, CommonMethods.ipIntToString((int)ipHeader.getDestinationIP()), tcpHeader.getDestinationPort(), flags, tcpHeader.toString());
		}
	}

	public void processSYNPacket(byte[] packet) {
		updateTCPBuffer(headerBytes, (byte) (TCPHeader.SYN | TCPHeader.ACK), mySeq, myAck, 0);
		client.sendToClient(headerBytes, 0, HEADER_SIZE);
		mySeq += 1;
		state = SYN_RCVD;
	}

	public void processACKPacket(byte[] packet) {
		switch (state) {
		case SYN_RCVD:
			processSYNRCVDACKPacket(packet);
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
			System.out.printf("TcpProxy(%d) connect close, state=%d.\n", id, state);
			break;
		default:
			System.out.printf("TcpProxy(%d) No function deal ACK packet.\n", id);
			break;
		}
	}

	public void processSYNRCVDACKPacket(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		if (tcpHeader.getSeqID() == myAck && tcpHeader.getAckID() == mySeq) {
			state = ESTABLISHED;
		}else {
			System.out.printf("TcpProxy(%d) Bad SYNRCVDACK.\n", id);
		}
	}

	public void processESTABLISHEDACKPacket(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int sequenceNumber = tcpHeader.getSeqID();	
		if (sequenceNumber == myAck) {
			// System.out.println("序列号匹配");
			int totalLength = ipHeader.getTotalLength();
			int headerLen = ipHeader.getHeaderLength() + tcpHeader.getHeaderLength();
			int dataSize = totalLength - headerLen;

			if (dataSize > 0) {
				writeToServer(packet, headerLen, dataSize);
				// 下一个序列号
				myAck += dataSize;
			}
			updateTCPBuffer(headerBytes, (byte) TCPHeader.ACK, mySeq, myAck, 0);
			// 往vpn客户端写ACK回复帧
			client.sendToClient(headerBytes, 0, HEADER_SIZE);

		} else if (sequenceNumber < myAck) {
			int dataSize = ipHeader.getTotalLength() - ipHeader.getHeaderLength()
					- tcpHeader.getHeaderLength();
			int nextSeq = sequenceNumber + dataSize;
			if (nextSeq > myAck) {
				System.out.printf("TcpProxy(%d) ACK seq %d max %d, 数据多包不处理.\n", id, nextSeq, myAck);
			} else {
				//System.out.printf("TcpProxy(%d) ACK seq %d min %d, 数据重传不处理.\n", id, nextSeq, myAck.get());
			}
		} else {
			System.out.printf("TcpProxy(%d) ACK seq %d max %d, 数据漏包不处理.\n", id, sequenceNumber, myAck);
		}
	}

	public void processRSTPacket(byte[] packet) {
		//System.out.printf("TcpProxy(%d) RST, 关闭连接.\n", id);
		destroy();
	}

	public void processURGPacket(byte[] packet) {

	}

	public void processPSHPacket(byte[] packet) {

	}

	public void processFINPacket(byte[] packet) {
		updateTCPBuffer(headerBytes, (byte)TCPHeader.ACK, mySeq, myAck, 0);
		client.sendToClient(headerBytes, 0, TcpProxy.HEADER_SIZE);
		state = CLOSE_WAIT;	
		processCLOSEWAITACKPacket(packet); 
	}
	
	public void processCLOSEWAITACKPacket(byte[] packet) {
		updateTCPBuffer(headerBytes, (byte)(TCPHeader.FIN | TCPHeader.ACK), mySeq, myAck, 0);
		client.sendToClient(headerBytes, 0, TcpProxy.HEADER_SIZE);
		state = LAST_ACK;
	}

	public void processLASTACKPacket(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int seq = tcpHeader.getAckID() - 1;
		int ack = tcpHeader.getSeqID() - 1;
		if (seq == mySeq && ack == myAck) {
			state = CLOSED;
			//System.out.printf("TcpProxy(%ld) LAST_ACK succeed, 关闭连接成功, seq %u:%u, ack %u:%u.\n", id, seq, mySeq, ack, myAck);
		} else {
			System.out.printf("TcpProxy(%ld) LAST_ACK failed, 队列号不匹配，seq %u:%u, ack %u:%u.\n", id, seq, mySeq, ack, myAck);
		}
	}

	public void disConnectClient() {
		//System.out.printf("TcpProxy(%ld) 开始主动关闭, state=%d.\n", id, state);
		destroy();
		updateTCPBuffer(headerBytes, (byte)TCPHeader.RST, mySeq, myAck, 0);
		client.sendToClient(headerBytes, 0, TcpProxy.HEADER_SIZE);
	}

	@Override
	public void run() {
		boolean noError = true;
		int size = 0;
		while (size != -1 && state != CLOSED && !closed && noError) {
			try {
				// 接收数据缓存
				byte[] bytes = new byte[Config.MUTE];
				size = is.read(bytes, HEADER_SIZE, Config.MUTE - HEADER_SIZE);
				synchronized (this) {
					if (size > 0) {
						CommonMethods.arraycopy(headerBytes, 0, bytes, 0, HEADER_SIZE);
						byte flag = TCPHeader.ACK;
						updateTCPBuffer(bytes, flag, mySeq, myAck, size);
						// 往客户端写ACK数据包
						noError = client.sendToClient(bytes, 0, size + HEADER_SIZE);
						// 更新已发送数据队列号
						mySeq += size;
					}
					lastRefreshTime = System.currentTimeMillis();
				}
			} catch (IOException e) {
				// TODO Auto-generated catch block
				//e.printStackTrace();
				noError = false;
			}
		}
		disConnectClient();
	}
	
	public boolean isExpire(){
		long now = System.currentTimeMillis();
		return (now - lastRefreshTime) > Config.PROXY_EXPIRE_TIME;
	}
	
	public boolean equal(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == tcpHeader.getSourcePort()
				&& destIp == ipHeader.getDestinationIP()
				&& destPort == tcpHeader.getDestinationPort();
	}
	
	public void updateTCPBuffer(byte[] packet, byte flag, int seq, int ack, int dataSize){
		IPHeader ipHeader = new IPHeader(packet, 0);
		identification++;
		ipHeader.setIdentification((short) identification);
		TCPHeader tcpHeader = new TCPHeader(packet, IPHeader.IP4_HEADER_SIZE);
		tcpHeader.setFlag(flag);
		tcpHeader.setSeqID(seq);
		tcpHeader.setAckID(ack);
		ipHeader.setTotalLength((short) (TcpProxy.HEADER_SIZE + dataSize));
		tcpHeader.ComputeTCPChecksum(ipHeader);
	}
	
}