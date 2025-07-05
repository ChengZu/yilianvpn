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
	private final int FIN_WAIT_1 = 5;
	private final int FIN_WAIT_2 = 6;
	private final int TIME_WAIT = 7;
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
		arraycopy(packet, 0, headerBytes, 0, IPHeader.IP4_HEADER_SIZE);
		arraycopy(packet, ipHeaderLen, headerBytes, IPHeader.IP4_HEADER_SIZE, TCPHeader.TCP_HEADER_SIZE);
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
		thread1.setName("TcpProxy(" + id + ") thread1");
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
			thread2 = new Thread(this, "TcpProxy(" + id + ") thread2");
			thread2.start();
			// 发送缓存的数据给目标服务器
			if (waitData.size() > 0) {
				for (int i = 0; i < waitData.size(); i++) {
					try {
						os.write(waitData.get(i), 0, waitData.get(i).length);
					} catch (IOException e) {
						// TODO Auto-generated catch block
						begainSelfClose();
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
				close();
				e.printStackTrace();
			}
		} else {
			byte[] data = new byte[size];
			System.arraycopy(packet, offset, data, 0, size);
			waitData.add(data);
		}
	}
	
	public boolean isClose() {
		return state == TIME_WAIT || state == CLOSED;
	}

	public void close() {
		closed = true;
		try {
			if(socket !=  null)
				socket.close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
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
		case FIN_WAIT_1:
			processFINWAIT1Packet(packet);
			break;
		case FIN_WAIT_2:
			processFINWAIT2Packet(packet);
			break;
		case TIME_WAIT:
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
		close();
		state = CLOSED;
	}

	public void processURGPacket(byte[] packet) {

	}

	public void processPSHPacket(byte[] packet) {

	}

	public void processFINPacket(byte[] packet) {
		close();
		if(isClose() || state == FIN_WAIT_1 || state == FIN_WAIT_2 || state == CLOSE_WAIT || state == LAST_ACK) return;
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		byte flags = TCPHeader.ACK;
		mySeq = tcpHeader.getAckID() - 1;
		myAck = tcpHeader.getSeqID() + 1;
		updateTCPBuffer(headerBytes, flags, mySeq, myAck, 0);
		client.sendToClient(headerBytes, 0, HEADER_SIZE);
		state = CLOSE_WAIT;
		processCLOSEWAITACKPacket(packet);
	}
	
	public void processCLOSEWAITACKPacket(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int seq = tcpHeader.getAckID() - 1;
		int ack = tcpHeader.getSeqID() + 1;
		if (ack == myAck && seq == mySeq) {
			updateTCPBuffer(headerBytes, (byte) (TCPHeader.FIN | TCPHeader.ACK), mySeq, myAck, 0);
			client.sendToClient(headerBytes, 0, HEADER_SIZE);
			state = LAST_ACK;
			//System.out.printf("TcpProxy(%d) CLOSE_WAIT succeed.\n", id);
		} else {
			System.out.printf("TcpProxy(%d) CLOSE_WAIT failed, seq %d:%d, ack %d:%d.\n", id, seq, mySeq, ack, myAck);
		}
	}

	public void processLASTACKPacket(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int seq = tcpHeader.getAckID() - 1;
		int ack = tcpHeader.getSeqID();
		if (ack == myAck && seq == mySeq) {
			state = CLOSED;
			//System.out.printf("TcpProxy(%d) LAST_ACK succeed, 关闭连接成功.\n", id);
		} else {
			System.out.printf("TcpProxy(%d) LAST_ACK failed, seq %d:%d, ack %d:%d.\n", id, seq, mySeq, ack, myAck);
		}
	}

	public void begainSelfClose() {		
		close();
		if(isClose() || state == CLOSE_WAIT || state == LAST_ACK || state == CLOSE_WAIT || state == LAST_ACK) return;
		updateTCPBuffer(headerBytes, (byte) TCPHeader.FIN, mySeq, myAck, 0);
		client.sendToClient(headerBytes, 0, HEADER_SIZE);
		state = FIN_WAIT_1;	
	}
	
	public void processFINWAIT1Packet(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int seq = tcpHeader.getAckID();
		int ack = tcpHeader.getSeqID();
		if (seq == mySeq && ack == myAck) {
			state = FIN_WAIT_2;
			//System.out.printf("TcpProxy(%d) FIN_WAIT_1 succeed.\n", id);
		} else {
			System.out.printf("TcpProxy(%d) FIN_WAIT_1 failed, seq %d:%d, ack %d:%d.\n", id, seq, mySeq, ack, myAck);
		}
	}

	public void processFINWAIT2Packet(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int seq = tcpHeader.getAckID();
		int ack = tcpHeader.getSeqID();
		if (seq == mySeq && ack == myAck) {
			updateTCPBuffer(headerBytes, (byte) TCPHeader.ACK, mySeq, (myAck + 1), 0);
			client.sendToClient(headerBytes, 0, HEADER_SIZE);
			state = TIME_WAIT;
			//System.out.printf("TcpProxy(%d) FIN_WAIT_2 succeed.\n", id);
		} else {
			System.out.printf("TcpProxy(%d) FIN_WAIT_2 failed, seq %d:%d, ack %d:%d.\n", id, seq, mySeq, ack, myAck);
		}
	}


	@Override
	public void run() {
		boolean noError = true;
		int size = 0;
		while (size != -1 && state != TIME_WAIT && state != CLOSED && !closed && noError) {
			try {
				// 接收数据缓存
				byte[] bytes = new byte[Config.MUTE];
				size = is.read(bytes, HEADER_SIZE, Config.MUTE - HEADER_SIZE);
				synchronized (this) {
					if (size > 0) {
						arraycopy(headerBytes, 0, bytes, 0, HEADER_SIZE);
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
		begainSelfClose();
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
	
	public void arraycopy(byte[] srcbuf, int srcoffset, byte[] desbuf, int desoffset, int size){
		for(int i=0; i<size; i++){
			desbuf[i+desoffset] = srcbuf[i+srcoffset];
		}
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
