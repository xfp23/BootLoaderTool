#include "IronBoot.h"
#include <fstream>
#include <vector>
#include <cstring>

static uint32_t CRC32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return ~crc;
}

IronBoot::IronBoot(ProtocolType_t type)
{
    this->proto = type;
}

IronBoot::~IronBoot()
{
}

void IronBoot::ReceiveFrameDataCallback(uint8_t *data, size_t data_len)
{

    switch (this->step)
    {
    case BlStep_t::WaitReset: // 复位阶段

        if (Portocol_UnPackCmdFrame(&this->RCF, data, data_len))
        {
            this->TimeoutCnt = 0; // 防止乱帧复位计数器,写进条件里面
            this->ErrCnt = 0; // 错误清0

            if (RCF.byte.Cmd != PROTOCOL_CMD_RESET)
            {
                this->step = BlStep_t::BlFail; // 复位失败
            }
            else
            {
                this->step = BlStep_t::BlStart; // 复位成功开始发送开始bl命令
            }
        }
        else
        {
            this->errStep = BlErrStep_t::ReTransCmd; // 错误帧，重发
        }
        break;

    case BlStep_t::WaitBlStart:  // 等待开始bl解锁
    case BlStep_t::WaitEarseApp: // 等待擦除APP响应
    case BlStep_t::WaitReadyBin: // 等待Slave准备接收固件前的准备
    case BlStep_t::WaitCheckBin: // 等待Slave校验固件

        if (Portocol_UnPackAckFrame(&this->RAF, data, data_len))
        {

            this->TimeoutCnt = 0; // 防止乱帧复位计数器,写进条件里面
            this->ErrCnt = 0; // 错误清除

            switch (_htons(this->CF.byte.Cmd))
            {
            case PROTOCOL_CMD_STARTBL: // bl解锁命令 // 解锁做简单一点，就固定数字

                if (this->RAF.byte.Ack == IronBoot::BlUnlock)
                {
                    this->step = BlStep_t::EarseApp; // 擦除app
                }
                else
                {
                    this->step = BlStep_t::BlFail; // 解锁失败
                }
                break;

            case PROTOCOL_CMD_EARSE:                          // 擦除APP命令
                if (this->RAF.byte.Ack == PROTOCOL_CMD_EARSE) // 擦除成功
                {
                    this->step = BlStep_t::ReadyBin; // 告诉从机准备接收二进制文件
                }
                else
                {
                    this->step = BlStep_t::BlFail; // 失败
                }
                break;

            case PROTOCOL_CMD_BIN:
                if (this->RAF.byte.Ack == PROTOCOL_CMD_BIN)
                {
                    this->step = BlStep_t::TransBin;
                }
                else
                {
                    this->step = BlStep_t::BlFail; // 失败
                }
                break;

            case PROTOCOL_CMD_CHECKBIN:
                if (this->RAF.byte.Ack == this->CF.byte.Cmd) // 从机把CRC32校验码应答上来
                {
                    // 固件校验成功
                    this->step = BlStep_t::BlSucc;
                }
                else
                {
                    this->step = BlStep_t::BlFail; // 固件校验失败，直接失败
                }
                break;
            }
        }
        else
        {
            this->errStep = BlErrStep_t::ReTransAck; // 失败重发
        }
        break;

    case BlStep_t::WaitBin:

        if (Portocol_UnPackDataFrame(&this->RDF, data, data_len))
        {
            this->TimeoutCnt = 0; // 防止乱帧复位计数器,写进条件里面
            if (this->RDF.byte.CRC == this->DF.byte.CRC && this->RDF.byte.Seq == this->DF.byte.Seq)
            {
                this->ErrCnt = 0;
                this->seq.seq++;

                if (this->seq.seq >= this->seq.total)
                {
                    this->step = BlStep_t::CkeckBin; // 全部发完
                }
                else
                {
                    this->step = BlStep_t::TransBin;
                }
            }
            else
            {
                // 数据传输有误，重发
                this->errStep = BlErrStep_t::ReTransData;
            }
        }
        else
        {
            // 解包失败,尝试重发
            this->errStep = BlErrStep_t::ReTransData;
        }
        break;

    // case BlStep_t::WaitCheckBin:

    // break;
    default:
        break;
    }
}

// 开始boot流程
void IronBoot::StartBl()
{

    this->step = BlStep_t::Reset;
    this->IsRun = true;
}

void IronBoot::RegisterTransmitCallback(TransmitDataFunc_t func)
{
    this->TransmitCallback = func;
}

void IronBoot::Timer1msHandler()
{
    if (this->step != BlStep_t::Idle)
    {
       if (++this->TimeoutCnt >= 10000)
       {
           this->step = BlStep_t::Timeout;
       }
    }
    else
    {
       this->TimeoutCnt = 0;
    }

    switch (this->errStep)
    {

    case BlErrStep_t::Idle:
        // 空闲
    break;
    case BlErrStep_t::ReTransCmd:
        if(this->ErrCnt++ < 5)
        {
            this->TransmitCallback(this->CF.data, sizeof(Protocol_CmdFrame_t));
            this->errStep = BlErrStep_t::Idle;
            printf("CF Data transmission error, %dth retransmission",this->ErrCnt);
            return;
        } else {
            this->step = BlStep_t::BlFail;
            this->ErrCnt = 0;
        }

        break;
    case BlErrStep_t::ReTransAck:
        if(this->ErrCnt++ < 5)
        {
            this->TransmitCallback(this->AF.data,sizeof(Protocol_AckFrame_t));
            this->errStep = BlErrStep_t::Idle;
            printf("AF Data transmission error, %dth retransmission",this->ErrCnt);
            return;
        } else {
            this->step = BlStep_t::BlFail;
            this->ErrCnt = 0;
        }
    break;

    case BlErrStep_t::ReTransData:
        if(this->ErrCnt++ < 5)
        {
            this->TransmitCallback(this->DF.data,sizeof(Protocol_DataFrame_t));
            this->errStep = BlErrStep_t::Idle;
            printf("DF Data transmission error, %dth retransmission",this->ErrCnt);
            return;
        } else {
            this->step = BlStep_t::BlFail;
            this->ErrCnt = 0;
        }
    break;
        break;
    default:
        break;
    }
    switch (this->step)
    {
    case BlStep_t::Idle:
        this->TimeoutCnt = 0;
        this->IsRun = false;

        break;
    case BlStep_t::Reset: 
        // 打包命令帧
        Portocol_PackCmdFrame(&this->CF, PROTOCOL_CMD_RESET);
        this->TransmitCallback(this->CF.data, sizeof(this->CF));
        this->step = BlStep_t::WaitReset;
        break;

    case BlStep_t::BlStart:
        Portocol_PackCmdFrame(&this->CF, PROTOCOL_CMD_STARTBL);
        this->TransmitCallback(this->CF.data, sizeof(Protocol_CmdFrame_t));
        this->step = BlStep_t::WaitBlStart;
        break;

    case BlStep_t::EarseApp: // 擦除app
        Portocol_PackCmdFrame(&this->CF, PROTOCOL_CMD_EARSE);
        this->TransmitCallback(this->CF.data, sizeof(Protocol_CmdFrame_t));
        this->step = BlStep_t::WaitEarseApp;
        break;
    case BlStep_t::ReadyBin: // 准备接收二进制
        this->seq.seq = 0;
        this->seq.total = (this->bin.len + IronBoot::DFBinLen - 1) / IronBoot::DFBinLen;
        this->seq.last_len = this->bin.len % IronBoot::DFBinLen;

        if (this->seq.last_len == 0)
        this->seq.last_len = IronBoot::DFBinLen;

        Portocol_PackCmdFrame(&this->CF, PROTOCOL_CMD_BIN);
        this->TransmitCallback(this->CF.data, sizeof(Protocol_CmdFrame_t));
        this->step = BlStep_t::WaitReadyBin;
        break;

    case BlStep_t::TransBin:
    {
        uint16_t current_len = IronBoot::DFBinLen;
        // 判断是否是最后一帧
        if (this->seq.seq == this->seq.total - 1)
        {
            current_len = this->seq.last_len;
        }

        Portocol_PackDataFrame(&this->DF, this->seq.seq,
                               &bin.bin[DFBinLen * this->seq.seq], current_len);
        this->TransmitCallback(this->DF.data, sizeof(Protocol_DataFrame_t));

        this->step = BlStep_t::WaitBin;
        break;
    }

    case BlStep_t::CkeckBin:

        Portocol_PackCmdFrame(&this->CF, PROTOCOL_CMD_CHECKBIN);
        this->TransmitCallback(this->CF.data, sizeof(Protocol_CmdFrame_t));
        this->step = BlStep_t::WaitCheckBin;

        break;
    case BlStep_t::BlFail:
        // 失败
        this->step = BlStep_t::Idle;
        this->errStep = BlErrStep_t::Idle;
        Portocol_PackCmdFrame(&this->CF, PROTOCOL_CMD_FAIL);
        this->TransmitCallback(this->CF.data, sizeof(Protocol_CmdFrame_t));
        printf("Upgrade Failed\n");
        memset((void*)&this->CF, 0, sizeof(Protocol_CmdFrame_t));
        memset((void*)&this->RCF, 0, sizeof(Protocol_CmdFrame_t));
        memset((void*)&this->AF, 0, sizeof(Protocol_AckFrame_t));
        memset((void*)&this->RAF, 0, sizeof(Protocol_AckFrame_t));
        memset((void*)&this->DF, 0, sizeof(Protocol_DataFrame_t));
        memset((void*)&this->RDF, 0, sizeof(Protocol_DataFrame_t));
        break;

    case BlStep_t::Timeout:

        printf("Transmission failed, please check the data link !\n");
        this->step = BlStep_t::Idle;
        break;

    case BlStep_t::BlSucc:
        Portocol_PackCmdFrame(&this->CF, PROTOCOL_CMD_SUCC);
        this->TransmitCallback(this->CF.data, sizeof(Protocol_CmdFrame_t)); // 从机可以写魔术了
        this->step = BlStep_t::Idle; // 成功后空闲
        printf("Upgrade successful\n");
        break;
    default:
        break;
    }
}

bool IronBoot::S19ToBin(uint8_t *data, size_t len)
{

    struct Seg
    {
        uint32_t addr;
        std::vector<uint8_t> data;
    };

    std::vector<Seg> segs;

    size_t i = 0;

    auto hex = [](uint8_t c) -> uint8_t
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return 0;
    };

    auto byte = [&](size_t pos) -> uint8_t
    {
        return (hex(data[pos]) << 4) | hex(data[pos + 1]);
    };

    while (i < len)
    {
        if (data[i] != 'S')
        {
            i++;
            continue;
        }

        char type = data[i + 1];
        i += 2;

        uint8_t count = byte(i);
        i += 2;

        int addrBytes = 0;

        if (type == '1')
            addrBytes = 2;
        else if (type == '2')
            addrBytes = 3;
        else if (type == '3')
            addrBytes = 4;
        else
        {
            while (i < len && data[i] != '\n')
                i++;
            continue;
        }

        uint32_t addr = 0;

        for (int j = 0; j < addrBytes; j++)
        {
            addr = (addr << 8) | byte(i);
            i += 2;
        }

        int dataLen = count - addrBytes - 1;

        Seg s;

        s.addr = addr;
        s.data.resize(dataLen);

        for (int j = 0; j < dataLen; j++)
        {
            s.data[j] = byte(i);
            i += 2;
        }

        segs.push_back(std::move(s));

        i += 2; // CRC
    }

    if (segs.empty())
        return false;

    uint32_t minAddr = segs[0].addr;
    uint32_t maxAddr = segs[0].addr;

    for (auto &s : segs)
    {
        if (s.addr < minAddr)
            minAddr = s.addr;

        if (s.addr + s.data.size() > maxAddr)
            maxAddr = s.addr + s.data.size();
    }

    size_t size = maxAddr - minAddr;

    std::vector<uint8_t> image(size, 0xFF);

    for (auto &s : segs)
    {
        uint32_t offset = s.addr - minAddr;

        memcpy(&image[offset], s.data.data(), s.data.size());
    }

    bin.baseAddr = minAddr;
    bin.len = image.size();

    bin.bin = new uint8_t[bin.len];

    memcpy(bin.bin, image.data(), bin.len);

    bin.crc = CRC32(bin.bin, bin.len);

    return true;
}

bool IronBoot::HexToBin(uint8_t *data, size_t len)
{
    struct Seg
    {
        uint32_t addr;
        std::vector<uint8_t> data;
    };

    std::vector<Seg> segs;

    uint32_t extAddr = 0;
    size_t i = 0;

    auto hex = [](uint8_t c) -> uint8_t
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return 0;
    };

    auto byte = [&](size_t pos) -> uint8_t
    {
        return (hex(data[pos]) << 4) | hex(data[pos + 1]);
    };

    while (i < len)
    {
        if (data[i] != ':')
        {
            i++;
            continue;
        }

        i++;

        uint8_t reclen = byte(i);
        i += 2;

        uint16_t addr = (byte(i) << 8) | byte(i + 2);
        i += 4;

        uint8_t type = byte(i);
        i += 2;

        if (type == 0x00)
        {
            Seg s;
            s.addr = extAddr + addr;

            s.data.resize(reclen);

            for (int j = 0; j < reclen; j++)
            {
                s.data[j] = byte(i);
                i += 2;
            }

            segs.push_back(std::move(s));
        }
        else if (type == 0x04)
        {
            extAddr = (static_cast<uint32_t>(byte(i) << 8) | byte(i + 2)) << 16;
            i += 4;
        }
        else if (type == 0x01)
        {
            break;
        }
        else
        {
            i += reclen * 2;
        }

        i += 2;
    }

    if (segs.empty())
        return false;

    uint32_t minAddr = segs[0].addr;
    uint32_t maxAddr = segs[0].addr;

    for (auto &s : segs)
    {
        if (s.addr < minAddr)
            minAddr = s.addr;

        if (s.addr + s.data.size() > maxAddr)
            maxAddr = s.addr + s.data.size();
    }

    size_t size = maxAddr - minAddr;

    std::vector<uint8_t> image(size, 0xFF);

    for (auto &s : segs)
    {
        uint32_t offset = s.addr - minAddr;

        memcpy(&image[offset], s.data.data(), s.data.size());
    }

    bin.baseAddr = minAddr;
    bin.len = image.size();

    bin.bin = new uint8_t[bin.len];

    memcpy(bin.bin, image.data(), bin.len);

    bin.crc = CRC32(bin.bin, bin.len);

    return true;
}

bool IronBoot::LoadFirmware(FirmwareType_t type, char *FileName)
{
    std::ifstream file(FileName, std::ios::binary);

    if (!file.is_open())
        return false;

    if (bin.bin != nullptr)
    {
        delete[] bin.bin;
        bin.bin = nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t len = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(len);

    file.read((char *)buf.data(), len);

    file.close();

    if (type == FirmwareType_t::AUTO)
    {
        if (buf[0] == ':')
            type = FirmwareType_t::HEX;
        else if (buf[0] == 'S')
            type = FirmwareType_t::S19;
        else
            type = FirmwareType_t::BIN;
    }

    switch (type)
    {
    case FirmwareType_t::HEX:
        return HexToBin(buf.data(), len);

    case FirmwareType_t::S19:
        return S19ToBin(buf.data(), len);

    case FirmwareType_t::BIN:
    {
        bin.baseAddr = 0;
        bin.len = len;

        bin.bin = new uint8_t[len];

        memcpy(bin.bin, buf.data(), len);

        bin.crc = CRC32(bin.bin, bin.len);

        return true;
    }

    default:
        return false;
    }
}

bool IronBoot::IsRunning(void)
{
    return this->IsRun;
}