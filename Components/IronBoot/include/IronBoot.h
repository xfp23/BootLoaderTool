/**
 * @file IronBoot.h
 * @author xfp23
 * @brief bootloader主机
 * @version 0.1
 * @date 2026-03-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "stdint.h"
#include "Protocol.h"

class IronBoot
{
public:
    typedef enum
    {
        IOSTP,
        Boot,
    } ProtocolType_t;

    typedef enum
    {
        HEX,
        S19,
        BIN,
        AUTO,
    } FirmwareType_t;

    typedef bool (*TransmitDataFunc_t)(uint8_t *data, size_t len);
    IronBoot(ProtocolType_t type = ProtocolType_t::Boot);
    ~IronBoot();
    void ReceiveFrameDataCallback(uint8_t *data, size_t data_len);
    void StartBl();
    void RegisterTransmitCallback(TransmitDataFunc_t callback);
    void Timer1msHandler();
    bool LoadFirmware(FirmwareType_t type = FirmwareType_t::HEX,char* FileName = nullptr);
    bool IsRunning(void);

private:
    typedef enum
    {
        Idle,
        Reset,
        BlStart,
        WaitBlStart,
        // ReTransCmd, // 失败重发
        // ReTransAck,
        // ReTransData,
        WaitReset,
        BlFail, // 失败
        // WaitReTransCmd,
        // WaitReTransAck,
        // WaitReTransData,
        Timeout,
        EarseApp,
        WaitEarseApp, // 等待擦除APP
        ReadyBin,     // 准备发送二进制文件
        WaitReadyBin, // 等待
        TransBin,
        WaitBin,
        CkeckBin,
        WaitCheckBin,
        BlSucc, // 成功
    } BlStep_t;

    enum class BlErrStep_t: uint8_t
    {
        Idle,
        NONEERR,
        ReTransCmd, // 失败重发
        ReTransAck,
        ReTransData,

    } ; // 错误重发机制

typedef struct
{
    uint8_t *bin;
    size_t len;
    uint32_t crc;
    uint32_t baseAddr;   // 固件起始地址
} Bin_t;

typedef struct 
{
    uint16_t total; // 总的seq
    uint16_t seq; // 当前seq
    uint16_t last_len; // 最后一帧的长度
}BinSeq_t;

    ProtocolType_t proto;                // boot协议
    TransmitDataFunc_t TransmitCallback; // 发送回调

    Protocol_AckFrame_t AF;  // 应答帧
    Protocol_DataFrame_t DF; // 数据帧
    Protocol_CmdFrame_t CF;  // 命令帧

    Protocol_AckFrame_t RAF;
    Protocol_DataFrame_t RDF;
    Protocol_CmdFrame_t RCF;

    Bin_t bin = {0};
    static const uint16_t DFBinLen = PROTOCOL_DATAFRAME_BINLEN;
    static const uint16_t BlUnlock = 0xE0F6;
    bool IsRun = false;
    BinSeq_t seq = {0};
    BlStep_t step = BlStep_t::Idle;
    uint16_t ErrCnt = 0;
    BlErrStep_t errStep = BlErrStep_t::NONEERR;
    volatile uint32_t TimeoutCnt; // 超时计数
    // volatile uint8_t ReTransCnt = 0;

    bool HexToBin(uint8_t *data,size_t len);
    bool S19ToBin(uint8_t *data,size_t len);
};
