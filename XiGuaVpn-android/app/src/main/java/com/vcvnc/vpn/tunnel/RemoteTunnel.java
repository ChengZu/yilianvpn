package com.vcvnc.vpn.tunnel;

import com.vcvnc.vpn.utils.ProxyConfig;
import com.vcvnc.vpn.service.VpnService;
import com.vcvnc.vpn.tcpip.IPHeader;
import com.vcvnc.vpn.utils.DebugLog;

import java.io.IOException;

import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;

import com.vcvnc.xiguavpn.R;


public class RemoteTunnel implements Runnable {
    private static final String TAG = RemoteTunnel.class.getSimpleName();
    private final VpnService vpnService;
    private SocketChannel socketChannel;
    private boolean closed = false;
    private byte[] cacheBytes;
    private boolean haveCacheBytes = false;
    Thread thread = null;
    String ipAndPort;
    public RemoteTunnel(VpnService vpnService) {
        this.vpnService = vpnService;
    }

    public void start() {
        connectServer();
        thread = new Thread(this, TAG);
        thread.start();
    }

    public boolean connectServer() {
        ipAndPort = ProxyConfig.serverIp+":"+ProxyConfig.serverPort;
        DebugLog.iWithTag(TAG, "RemoteTunnel:%s connecting server.", ipAndPort);

        try {
            socketChannel = SocketChannel.open();
            socketChannel.configureBlocking(false);
            socketChannel.connect(new InetSocketAddress(ProxyConfig.serverIp, ProxyConfig.serverPort));
            vpnService.protect(socketChannel.socket());

            while (!socketChannel.finishConnect()){
                Thread.sleep(1);
            }

            closed = false;
            //发送用户信息
            byte[] header = new byte[IPHeader.IP4_HEADER_SIZE];
            IPHeader ipheader = new IPHeader(header, 0);
            ipheader.setHeaderLength(IPHeader.IP4_HEADER_SIZE);
            ipheader.setSourceIP(ProxyConfig.userName);
            ipheader.setDestinationIP(ProxyConfig.userPwd);
            ipheader.setProtocol((byte) IPHeader.TCP);
            socketChannel.write(ByteBuffer.wrap(header, 0, IPHeader.IP4_HEADER_SIZE));
            DebugLog.iWithTag(TAG, "RemoteTunnel:%s connect server succeed.", ipAndPort);

        } catch (IOException e) {
            DebugLog.dWithTag(TAG, "RemoteTunnel:%s connect server fail.", ipAndPort);
            close(vpnService.getString(R.string.can_not_connect_server));
            return false;
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        return true;
    }

    //发送给服务器
    public void processPacket(byte[] bytes, int size) {
        try {
            socketChannel.write(ByteBuffer.wrap(bytes, 0, size));
        } catch (IOException e) {
            DebugLog.wWithTag(TAG, "Network write error: %s %s", ipAndPort, e);
            close(vpnService.getString(R.string.send_packet_error));
        }
    }

    public void close(String errMsg) {
        if(closed) return;
        closed = true;
        ProxyConfig.errorMsg = errMsg;
        vpnService.setVpnRunningStatus(false);
        if(thread != null)
            thread.interrupt();
        try {
            socketChannel.close();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public boolean isClose() {
        return closed;
    }

    synchronized public void processRecvPacket(byte[] bytes, int size) {
        if(this.haveCacheBytes) {
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
        int totalLength = IpHeader.getTotalLength() & 0xFFFFFFFF;
        if(totalLength > ProxyConfig.MUTE){
            close(vpnService.getString(R.string.rev_bad_length_packet));
        }
        if(totalLength < size){
            vpnService.write(bytes, 0, totalLength);
            int nextDataSize = size - totalLength;
            byte[] data = new byte[nextDataSize];
            System.arraycopy(bytes, totalLength, data, 0, nextDataSize);
            processRecvPacket(data, nextDataSize);
        }else if(totalLength == size){
            vpnService.write(bytes, 0, size);
        }else if(totalLength > size){
            byte[] data = new byte[size];
            System.arraycopy(bytes, 0, data, 0, size);
            this.cacheBytes = data;
            this.haveCacheBytes = true;
        }
    }

    @Override
    public void run() {
        try {
            while (socketChannel.isOpen() && !closed) {
                byte[] bytes = new byte[ProxyConfig.MUTE];
                int size = socketChannel.read(ByteBuffer.wrap(bytes));
                if (size > 0) {
                    processRecvPacket(bytes, size);
                }
            }
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        close(vpnService.getString(R.string.connect_abort));
    }

}
