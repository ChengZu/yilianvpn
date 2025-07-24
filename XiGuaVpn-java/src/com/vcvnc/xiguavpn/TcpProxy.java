package com.vcvnc.xiguavpn;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.UnknownHostException;


public class TcpProxy extends Proxy implements Runnable {
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
	private boolean connected = false;
	// 初始化连接线程
	private Thread thread1;
	// 接收目标服务器数据线程
	private Thread thread2;
	
	private int myWindow = 65535;
	private int clientWindow = 65535;
	private RingQueueBuffer clientRingQueueBuffer = new RingQueueBuffer();
	private RingQueueBuffer serverRingQueueBuffer = new RingQueueBuffer();

	public static final int HEADER_SIZE = IPHeader.IP4_HEADER_SIZE + TCPHeader.TCP_HEADER_SIZE;

	public TcpProxy(Client client, byte[] packet) {
		super(client, packet);
		headerBytes = new byte[TcpProxy.HEADER_SIZE];
		IPHeader ipHeader = new IPHeader(headerBytes, 0);
		TCPHeader tcpHeader = new TCPHeader(headerBytes, IPHeader.IP4_HEADER_SIZE);

		ipHeader.setHeaderLength(IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTos((byte) 0x8); // 最大吞吐量 
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

	private void connect() {
		try {
			socket = new Socket(CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			is = socket.getInputStream();
			os = socket.getOutputStream();
			connected = true;
		} catch (UnknownHostException e) {
			// TODO Auto-generated catch block
			System.out.printf("TcpProxy(%d) Connect bad address %s:%d.\n", id, CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			// e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			System.out.printf("TcpProxy(%d) Connect error, address %s:%d.\n", id, CommonMethods.ipIntToInet4Address(destIp), destPort & 0xFFFF);
			// e.printStackTrace();
		}
		if (connected) {
			thread2 = new Thread(this, "TcpProxy(" + id + ") recv server data thread");
			thread2.start();
			// 发送缓存的数据给目标服务器
			int rlen = clientRingQueueBuffer.availableReadLength();
			if(rlen > 0){
				byte[] data = clientRingQueueBuffer.poll(rlen);
				try {
					os.write(data, 0, rlen);
				} catch (IOException e) {
					// TODO Auto-generated catch block
					clientRingQueueBuffer.push(data, rlen);
					e.printStackTrace();
				}
			}
		} else {
			close();
		}
	}

	public boolean sendToClient(byte[] packet, int size) {
		lastClientRefreshTime = System.currentTimeMillis();
		boolean ret = true;
		int rlen = serverRingQueueBuffer.availableReadLength();
		if(rlen > 0){
			
			byte[] data = serverRingQueueBuffer.poll(rlen);
			ret = client.sendToClient(data, 0, rlen);
			if(!ret) {
				serverRingQueueBuffer.push(packet, size);
			}
		}
		ret = client.sendToClient(packet, 0, size);
		if(!ret) {
			serverRingQueueBuffer.push(packet, size);
		}
		return ret;
	}
	
	public void sendToServer(byte[] packet, int size) {
		lastClientRefreshTime = System.currentTimeMillis();
		
		if (connected) {
			try {
				int rlen = clientRingQueueBuffer.availableReadLength();
				if(rlen > 0){
					byte[] data = clientRingQueueBuffer.poll(rlen);
					os.write(data, 0, rlen);
				}
				os.write(packet, 0, size);
			} catch (IOException e) {
				// TODO Auto-generated catch block
				clientRingQueueBuffer.push(packet, size);
				e.printStackTrace();
			}
		} else {
			clientRingQueueBuffer.push(packet, size);
		}
		myWindow = clientRingQueueBuffer.availableWriteLength();
	}
	
	@Override
	public boolean isClosed() {
		return state == CLOSED;
	}

	@Override
	public void close() {
		state = CLOSED;
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
	
	@Override
	public void processFisrtPacket(byte[] packet, int size) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		mySeq = 0;
		myAck = tcpHeader.getSeqID() + 1;
		int flags = tcpHeader.getFlag();
		// 第一个TCP包不是SYN, 销毁该代理 
		if((flags & TCPHeader.SYN) == TCPHeader.SYN) {
			thread1 = new Thread() {
				@Override
				public void run() {
					connect();
				}
			};
			thread1.setName("TcpProxy(" + id + ") connect server thread");
			thread1.start();
			processPacket(packet, size);
		}else {
			// System.out.printf("[TcpProxy]proxy(%s) recvive client first packet(%s) is not syn, flags is %d.\n", toString(), tcpHeader.toString(), flags);
			// 发送RST关闭客户端TCP连接(释放资源) 
			if((flags & TCPHeader.RST) == TCPHeader.RST){
				close();
			}else {
				disConnectClient();
			}
		}	
	}
	
	@Override
	public void processPacket(byte[] packet, int size) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		clientWindow = tcpHeader.getWindow() & 0xFFFF;
		int flags = tcpHeader.getFlag();
		
		if((flags & TCPHeader.SYN) == TCPHeader.SYN){
			processSYNPacket(packet);
		}else if((flags & TCPHeader.SYN) == TCPHeader.SYN && (flags & TCPHeader.ACK) == TCPHeader.ACK){
			processACKPacket(packet);
		}else if((flags & TCPHeader.FIN) == TCPHeader.FIN && (flags & TCPHeader.ACK) == TCPHeader.ACK){
			processFINPacket(packet);
		}else if((flags & TCPHeader.ACK) == TCPHeader.ACK){
			processACKPacket(packet);
		}else if((flags & TCPHeader.RST) == TCPHeader.RST){
			processRSTPacket(packet);
		}else if((flags & TCPHeader.PSH) == TCPHeader.PSH){
			processPSHPacket(packet);
		}else if((flags & TCPHeader.URG) == TCPHeader.URG){
			processURGPacket(packet);
		}else{
			System.out.printf("Client(%d) tcpProxy(%s:%d) 未处理标志位%d{%s}.\n", client.id, CommonMethods.ipIntToString((int)ipHeader.getDestinationIP()), tcpHeader.getDestinationPort(), flags, tcpHeader.toString());
		}
	}

	public void processSYNPacket(byte[] packet) {
		updateTCPBuffer(headerBytes, (byte) (TCPHeader.SYN | TCPHeader.ACK), mySeq, myAck, 0);
		sendToClient(headerBytes, HEADER_SIZE);
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
		int seq = tcpHeader.getSeqID();	
		if (seq == myAck) {
			// System.out.println("序列号匹配");
			int totalLength = ipHeader.getTotalLength();
			int headerLen = ipHeader.getHeaderLength() + tcpHeader.getHeaderLength();
			int dataSize = totalLength - headerLen;

			if (dataSize > 0) {
				byte[] data = new byte[dataSize];
				System.arraycopy(packet, headerLen, data, 0, dataSize);
				sendToServer(data, dataSize);
				// 下一个序列号
				myAck += dataSize;
			}
			updateTCPBuffer(headerBytes, (byte) TCPHeader.ACK, mySeq, myAck, 0);
			// 往vpn客户端写ACK回复帧
			sendToClient(headerBytes, HEADER_SIZE);

		} else if (seq < myAck) {
			int dataSize = ipHeader.getTotalLength() - ipHeader.getHeaderLength()
					- tcpHeader.getHeaderLength();
			int nextSeq = seq + dataSize;
			if (nextSeq > myAck) {
				System.out.printf("TcpProxy(%d) ACK seq %d max %d, more packets.\n", id, nextSeq, myAck);
			} else if(nextSeq < myAck){
				//System.out.printf("TcpProxy(%d) ACK seq %d min %d, repeat packets.\n", id, nextSeq, myAck);
			}
		} else {
			System.out.printf("TcpProxy(%d) ACK seq %d max %d, miss packets.\n", id, seq, myAck);
		}
	}

	public void processRSTPacket(byte[] packet) {
		//System.out.printf("TcpProxy(%d) RST, 关闭连接.\n", id);
		close();
	}

	public void processURGPacket(byte[] packet) {

	}

	public void processPSHPacket(byte[] packet) {

	}

	public void processFINPacket(byte[] packet) {
		updateTCPBuffer(headerBytes, (byte)TCPHeader.ACK, mySeq, myAck, 0);
		sendToClient(headerBytes, TcpProxy.HEADER_SIZE);
		state = CLOSE_WAIT;	
		processCLOSEWAITACKPacket(packet); 
	}
	
	public void processCLOSEWAITACKPacket(byte[] packet) {
		updateTCPBuffer(headerBytes, (byte)(TCPHeader.FIN | TCPHeader.ACK), mySeq, myAck, 0);
		sendToClient(headerBytes, TcpProxy.HEADER_SIZE);
		state = LAST_ACK;
	}

	public void processLASTACKPacket(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		int ack = tcpHeader.getAckID() - 1;
		int seq = tcpHeader.getSeqID() - 1;
		if (ack == mySeq && seq == myAck) {
			close();
			//System.out.printf("TcpProxy(%d) LAST_ACK succeed, closed, seq %d:%d, ack %d:%d.\n", id, seq %d:%d, ack %d:%d.\n", id, seq, myAck, ack, mySeq);
		} else {
			System.out.printf("TcpProxy(%d) LAST_ACK failed, queue number mismatched, seq %d:%d, ack %d:%d.\n", id, seq, myAck, ack, mySeq);
		}
	}

	public void disConnectClient() {
		//System.out.printf("TcpProxy(%d) 开始主动关闭, state=%d.\n", id, state);
		close();
		updateTCPBuffer(headerBytes, (byte)TCPHeader.RST, mySeq, myAck, 0);
		sendToClient(headerBytes, TcpProxy.HEADER_SIZE);
	}

	@Override
	public void run() {
		boolean noError = true;
		int size = 0;
		while (size != -1 && state != CLOSED && !closed && noError) {
			try {
				
				int len = Config.MUTE - HEADER_SIZE;
				int cw = clientWindow - serverRingQueueBuffer.availableReadLength();
				len = cw < len ? cw : len;
				if(len <= 0){
					continue;
				}
				
				// 接收数据缓存
				byte[] bytes = new byte[len + HEADER_SIZE];
				size = is.read(bytes, HEADER_SIZE, len);
				synchronized (this) {
					if (size > 0) {
						CommonMethods.arraycopy(headerBytes, 0, bytes, 0, HEADER_SIZE);
						byte flag = TCPHeader.ACK;
						updateTCPBuffer(bytes, flag, mySeq, myAck, size);
						// 往客户端写ACK数据包
						noError = sendToClient(bytes, size + HEADER_SIZE);
						// 更新已发送数据队列号
						mySeq += size;
					}
					lastServerRefreshTime = System.currentTimeMillis();
				}
			} catch (IOException e) {
				// TODO Auto-generated catch block
				//e.printStackTrace();
				noError = false;
			}
		}
		close();
	}
	
	@Override
	public boolean equal(byte[] packet) {
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, ipHeader.getHeaderLength());
		return srcIp == ipHeader.getSourceIP() && srcPort == tcpHeader.getSourcePort() && destIp == ipHeader.getDestinationIP() && destPort == tcpHeader.getDestinationPort();
	}
	
	public void updateTCPBuffer(byte[] packet, byte flag, int seq, int ack, int dataSize){
		identification++;
		IPHeader ipHeader = new IPHeader(packet, 0);
		TCPHeader tcpHeader = new TCPHeader(packet, IPHeader.IP4_HEADER_SIZE);
		ipHeader.setTotalLength((short) (TcpProxy.HEADER_SIZE + dataSize));
		ipHeader.setIdentification((short) identification);
		tcpHeader.setFlag(flag);
		tcpHeader.setSeqID(seq);
		tcpHeader.setAckID(ack);
		tcpHeader.setWindow((short) myWindow);
		tcpHeader.ComputeTCPChecksum(ipHeader);
	}
}