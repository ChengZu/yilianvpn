#ifndef UDPHEADER_H
#define UDPHEADER_H
#include "CommonMethods.h"
#include "IPHeader.h"

class UDPHeader {
    /**
     * UDP数据报格式
     * 头部长度：8字节
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜  １６位源端口号         ｜   １６位目的端口号            ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜  １６位ＵＤＰ长度       ｜   １６位ＵＤＰ检验和          ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜                  数据（如果有）                          ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     **/
public:
	static const int UDP_HEADER_SIZE = 8;
	static const short offset_src_port = 0; // 源端口 
	static const short offset_dest_port = 2; // 目的端口 
	static const short offset_tlen = 4; // 数据报长度
	static const short offset_crc = 6; // 校验和

	char* mData;
	int mOffset;

	UDPHeader(){
	}

	UDPHeader(char* data, int offset) {
		mData = data;
		mOffset = offset;
	}

	unsigned short getSourcePort() {
		return CommonMethods::readShort(mData, mOffset + offset_src_port) & 0xFFFF;
	}

	void setSourcePort(short value) {
		CommonMethods::writeShort(mData, mOffset + offset_src_port, value);
	}

	unsigned short getDestinationPort() {
		return CommonMethods::readShort(mData, mOffset + offset_dest_port) & 0xFFFF;
	}

	void setDestinationPort(short value) {
		CommonMethods::writeShort(mData, mOffset + offset_dest_port, value);
	}

	unsigned short getTotalLength() {
		return CommonMethods::readShort(mData, mOffset + offset_tlen) & 0xFFFF;
	}

	void setTotalLength(short value) {
		CommonMethods::writeShort(mData, mOffset + offset_tlen, value);
	}

	unsigned short getCrc() {
		return CommonMethods::readShort(mData, mOffset + offset_crc) & 0xFFFF;
	}

	void setCrc(short value) {
		CommonMethods::writeShort(mData, mOffset + offset_crc, value);
	}
    
	// 计算UDP校验和 
	// UDP检验和 = 整个UDP报文（不合检验和部分） +  源地址 + 目标地址 + 协议 + UDP报文长度 
	bool ComputeUDPChecksum(IPHeader ipHeader) {
		ipHeader.ComputeIPChecksum();
		int ipData_len = ipHeader.getDataLength();
		if (ipData_len < 0) {
			return false;
		}

		// 计算伪首部和 
		long sum = ipHeader.getsum(ipHeader.mData, ipHeader.mOffset + IPHeader::offset_src_ip, 8);
		sum += ipHeader.getProtocol() & 0xFF;
		sum += ipData_len;
		short oldCrc = getCrc();
		setCrc((short) 0);

		short newCrc = ipHeader.checksum(sum, mData, mOffset, ipData_len);

		setCrc(newCrc);
		return oldCrc == newCrc;
	}

	std::string toString() {
		std::stringstream ss;
		ss  << getSourcePort()
			<< "->"
			<< getDestinationPort();
		return ss.str();
	}

};

#endif
