#ifndef IPHEADER_H
#define IPHEADER_H
#include "CommonMethods.h"

class IPHeader
{

    /**
     * IP报文格式
     * 0                                   　　　　           15  16　　　　　　　　　　　　　　　　　　　　　　　　     31
     * ｜　－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜  ４　位     ｜   ４位首     ｜      ８位服务类型      ｜      　　         １６位总长度            　            ｜
     * ｜  版本号     ｜   部长度     ｜      （ＴＯＳ）　      ｜      　 　 （ｔｏｔａｌ　ｌｅｎｇｔｈ）    　           ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜  　　　　　　　　１６位标识符                         ｜　３位    ｜　　　　１３位片偏移                         ｜
     * ｜            （ｉｎｄｅｎｔｉｆｉｅｒ）                 ｜　标志    ｜      （ｏｆｆｓｅｔ）　　                   ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜      ８位生存时间ＴＴＬ    ｜       ８位协议          ｜　　　　　　　　１６位首部校验和                         ｜
     * ｜（ｔｉｍｅ　ｔｏ　ｌｉｖｅ）｜  （ｐｒｏｔｏｃｏｌ）   ｜              （ｃｈｅｃｋｓｕｍ）                       ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜                                ３２位源ＩＰ地址（ｓｏｕｒｃｅ　ａｄｄｒｅｓｓ）                                  ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜                           ３２位目的ＩＰ地址（ｄｅｓｔｉｎａｔｉｏｎ　ａｄｄｒｅｓｓ）                           ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜                                             ３２位选项（若有）                                                   ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     * ｜                                                                                                                  ｜
     * ｜                                                  数据                                                            ｜
     * ｜                                                                                                                  ｜
     * ｜－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－｜
     **/
public:
    static const int IP4_HEADER_SIZE = 20;
    static const short IP = 0x0800;
    static const char ICMP = 1;
    static const char TCP = 6;  // 6: TCP协议号
    static const char UDP = 17; // 17: UDP协议号
    static const char offset_proto = 9; // 9：8位协议偏移
    static const int offset_src_ip = 12; // 12：源ip地址偏移
    static const int offset_dest_ip = 16; // 16：目标ip地址偏移
    static const char offset_ver_ihl = 0; // 0: 版本号（4bits） + 首部长度（4bits）
    static const char offset_tos = 1; // 1：服务类型偏移
    static const short offset_tlen = 2; // 2：总长度偏移
    static const short offset_identification = 4; // 4：16位标识符偏移
    static const short offset_flags_fo = 6; // 6：标志（3bits）+ 片偏移（13bits）
    static const char offset_ttl = 8; // 8：生存时间偏移
    static const short offset_crc = 10; // 10：首部校验和偏移
    static const int offset_op_pad = 20; // 20：选项 + 填充

    char *mData;
    int mOffset;

    IPHeader()
    {
    }

    IPHeader(char *data, int offset)
    {
        mData = data;
        mOffset = offset;
    }

    void Default()
    {
        setHeaderLength(20);
        setTos((char) 0);
        setTotalLength(0);
        setIdentification(0);
        setFlagsAndOffset((short) 0);
        setTTL((char) 64);
    }

    unsigned int getDataLength()
    {
        return getTotalLength() - getHeaderLength();
    }

    unsigned int getHeaderLength()
    {
        return (mData[mOffset + offset_ver_ihl] & 0x0F) * 4;
    }

    void setHeaderLength(int value)
    {
        // 4 << 4 表示版本为IPv4
        mData[mOffset + offset_ver_ihl] = (char) ((4 << 4) | (value / 4));
    }

    unsigned char getTos()
    {
        return mData[mOffset + offset_tos] & 0xFF;
    }

    void setTos(char value)
    {
        mData[mOffset + offset_tos] = value;
    }

    unsigned short getTotalLength()
    {
        return CommonMethods::readShort(mData, mOffset + offset_tlen) & 0xFFFF;
    }

    void setTotalLength(short value)
    {
        CommonMethods::writeShort(mData, mOffset + offset_tlen, value);
    }

    unsigned short getIdentification()
    {
        return CommonMethods::readShort(mData, mOffset + offset_identification) & 0xFFFF;
    }

    void setIdentification(short value)
    {
        CommonMethods::writeShort(mData, mOffset + offset_identification, value);
    }

    unsigned short getFlagsAndOffset()
    {
        return CommonMethods::readShort(mData, mOffset + offset_flags_fo) & 0xFFFF;
    }

    void setFlagsAndOffset(short value)
    {
        CommonMethods::writeShort(mData, mOffset + offset_flags_fo, value);
    }

    unsigned char getTTL()
    {
        return mData[mOffset + offset_ttl] & 0xFF;
    }

    void setTTL(char value)
    {
        mData[mOffset + offset_ttl] = value;
    }

    unsigned char getProtocol()
    {
        return mData[mOffset + offset_proto] & 0xFF;
    }

    void setProtocol(char value)
    {
        mData[mOffset + offset_proto] = value;
    }

    unsigned short getCrc()
    {
        return CommonMethods::readShort(mData, mOffset + offset_crc) & 0xFFFF;
    }

    void setCrc(short value)
    {
        CommonMethods::writeShort(mData, mOffset + offset_crc, value);
    }

    unsigned int getSourceIP()
    {
        return CommonMethods::readInt(mData, mOffset + offset_src_ip) & 0xFFFFFFFF;
    }

    void setSourceIP(int value)
    {
        CommonMethods::writeInt(mData, mOffset + offset_src_ip, value);
    }

    unsigned int getDestinationIP()
    {
        return CommonMethods::readInt(mData, mOffset + offset_dest_ip) & 0xFFFFFFFF;
    }

    void setDestinationIP(int value)
    {
        CommonMethods::writeInt(mData, mOffset + offset_dest_ip, value);
    }

    //计算校验和
    short checksum(long sum, char *buf, int offset, int len)
    {
        sum += getsum(buf, offset, len);
        while ((sum >> 16) > 0)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        return (short) ~sum;
    }

    long getsum(char *buf, int offset, int len)
    {
        long sum = 0;
        while (len > 1)
        {
            sum += CommonMethods::readShort(buf, offset) & 0xFFFF;
            offset += 2;
            len -= 2;
        }

        if (len > 0)
        {
            sum += (buf[offset] & 0xFF) << 8;
        }

        return sum;
    }

    // 计算IP包的校验和
    bool ComputeIPChecksum()
    {
        short oldCrc = getCrc();
        setCrc((short) 0);
        short newCrc = checksum(0, mData, mOffset, getHeaderLength());
        setCrc(newCrc);
        return oldCrc == newCrc;
    }

    std::string toString()
    {
        std::stringstream ss;
        ss  << CommonMethods::ipIntToString(getSourceIP())
            << "->"
            << CommonMethods::ipIntToString(getDestinationIP())
            << " Pro="
            << (int)getProtocol()
            << ", Hlen="
            << getHeaderLength();
        return ss.str();
    }
};

#endif
