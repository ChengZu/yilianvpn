package com.vcvnc.xiguavpn;


public class UDPHeader {
    /**
     * UDP数据报格式
     * 头部长度：8字节
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜  １６位源端口号         ｜   １６位目的端口号        ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜  １６位ＵＤＰ长度       ｜   １６位ＵＤＰ检验和       ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜                  数据（如果有）                    ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     **/

    public static final int UDP_HEADER_SIZE = 8;
    public static final short offset_src_port = 0; // 源端口
    public static final short offset_dest_port = 2; //目的端口
    public static final short offset_tlen = 4; //数据报长度
    public static final short offset_crc = 6; //校验和

    public byte[] mData;
    public int mOffset;

    public UDPHeader(byte[] data, int offset) {
        mData = data;
        mOffset = offset;
    }

    public short getSourcePort() {
        return CommonMethods.readShort(mData, mOffset + offset_src_port);
    }

    public void setSourcePort(short value) {
        CommonMethods.writeShort(mData, mOffset + offset_src_port, value);
    }

    public short getDestinationPort() {
        return CommonMethods.readShort(mData, mOffset + offset_dest_port);
    }

    public void setDestinationPort(short value) {
        CommonMethods.writeShort(mData, mOffset + offset_dest_port, value);
    }

    public short getTotalLength() {
        return CommonMethods.readShort(mData, mOffset + offset_tlen);
    }

    public void setTotalLength(short value) {
        CommonMethods.writeShort(mData, mOffset + offset_tlen, value);
    }

    public short getCrc() {
        return CommonMethods.readShort(mData, mOffset + offset_crc);
    }

    public void setCrc(short value) {
        CommonMethods.writeShort(mData, mOffset + offset_crc, value);
    }

    //计算UDP校验和
    //UDP检验和 = 整个UDP报文（不合检验和部分） +  源地址 + 目标地址 + 协议 + UDP报文长度
    public boolean ComputeUDPChecksum(IPHeader ipHeader) {
        ipHeader.ComputeIPChecksum();
        int ipData_len = ipHeader.getDataLength();
        if (ipData_len < 0) {
            return false;
        }

        //计算伪首部和
        long sum = ipHeader.getsum(ipHeader.mData, ipHeader.mOffset + IPHeader.offset_src_ip, 8);
        sum += ipHeader.getProtocol() & 0xFF;
        sum += ipData_len;
        short oldCrc = getCrc();
        setCrc((short) 0);

        short newCrc = ipHeader.checksum(sum, mData, mOffset, ipData_len);

        setCrc(newCrc);
        return oldCrc == newCrc;
    }

	@Override
	public String toString() {
		return String.format("%d->%d", getSourcePort() & 0xFFFF, getDestinationPort() & 0xFFFF);
	}
}
