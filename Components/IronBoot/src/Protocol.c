#include "Protocol.h"
#include <stdlib.h>
#include <string.h>

#define PROTOCOL_SOF (0xAA55)

/* ---------------- CRC16 (CCITT-FALSE) ---------------- */
static uint16_t CRC16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ---------------- 数据帧 (DataFrame) ---------------- */

bool Portocol_PackDataFrame(Protocol_DataFrame_t *dst, uint16_t seq, uint8_t *data, uint16_t data_len)
{
    if (dst == NULL || data_len > PROTOCOL_DATAFRAME_BINLEN || data == NULL)
        return false;

    memset(dst, 0, sizeof(Protocol_DataFrame_t));

    dst->byte.SOF  = _htons(PROTOCOL_SOF);
    dst->byte.Type = (uint8_t)FrameType_Data;
    dst->byte.Len  = _htons(data_len);
    dst->byte.Seq  = _htons(seq);
    
    memcpy(dst->byte.Bin, data, data_len);

    // 校验范围: Type, Len, Seq, Bin 共 1 + 2 + 2 + 8 = 13 字节
    // 对应 dst->data[2] 到 CRC 之前
    uint16_t crc_val = CRC16(&dst->data[2], PROTOCOL_DATAFRAME_SIZE - 4);
    dst->byte.CRC = _htons(crc_val);

    return true;
}

bool Portocol_UnPackDataFrame(Protocol_DataFrame_t* dst, uint8_t* data, size_t len)
{
    if (dst == NULL || data == NULL || len != PROTOCOL_DATAFRAME_SIZE)
        return false;

    memcpy(dst->data, data, len);

    // 1. 校验 SOF (判断时转一下)
    if (_ntohs(dst->byte.SOF) != PROTOCOL_SOF || dst->byte.Type != FrameType_Data)
        return false;

    // 2. 校验 CRC (对比时转一下)
    uint16_t crc_calc = CRC16(&dst->data[2], PROTOCOL_DATAFRAME_SIZE - 4);
    if (crc_calc != _ntohs(dst->byte.CRC))
        return false;

    // 3. 【关键：赋值还原】既然校验过了，把所有多字节字段转回主机序
    // 这样你外层代码拿到 dst 就可以直接用了
#ifdef PROTOCOL_HTONS
    dst->byte.SOF = _ntohs(dst->byte.SOF);
    dst->byte.Len = _ntohs(dst->byte.Len);
    dst->byte.Seq = _ntohs(dst->byte.Seq);
    dst->byte.CRC = _ntohs(dst->byte.CRC);
#endif

    return true;
}

/* ---------------- 命令帧 (CmdFrame) ---------------- */

bool Portocol_PackCmdFrame(Protocol_CmdFrame_t *dst, Protocol_CmdAck_t cmd)
{
    if (dst == NULL) return false;

    memset(dst, 0, sizeof(Protocol_CmdFrame_t));

    dst->byte.SOF  = _htons(PROTOCOL_SOF);
    dst->byte.Type = (uint8_t)FrameType_Cmd;
    dst->byte.Len  = 2; // 命令固定2字节
    dst->byte.Cmd  = _htons((uint16_t)cmd);

    // 校验范围: Type, Len, Cmd 共 1 + 1 + 2 = 4 字节
    uint16_t crc_val = CRC16(&dst->data[2], 8 - 4);
    dst->byte.CRC = _htons(crc_val);

    return true;
}

bool Portocol_UnPackCmdFrame(Protocol_CmdFrame_t* dst, uint8_t* data, size_t data_len)
{
    if (dst == NULL || data == NULL || data_len != 8)
        return false;

    memcpy(dst->data, data, data_len);

    if (_ntohs(dst->byte.SOF) != PROTOCOL_SOF || dst->byte.Type != FrameType_Cmd)
        return false;

    uint16_t crc_calc = CRC16(&dst->data[2],sizeof(Protocol_CmdFrame_t) - 4);
    if (crc_calc != _ntohs(dst->byte.CRC))
        return false;

    // 【赋值还原】
#ifdef PROTOCOL_HTONS
    dst->byte.SOF = _ntohs(dst->byte.SOF);
    dst->byte.Cmd = _ntohs(dst->byte.Cmd);
    dst->byte.CRC = _ntohs(dst->byte.CRC);
#endif

    return true;
}

/* ---------------- 应答帧 (AckFrame) ---------------- */

bool Portocol_PackAckFrame(Protocol_AckFrame_t *dst, uint32_t Ack)
{
    if (dst == NULL) return false;

    memset(dst, 0, sizeof(Protocol_AckFrame_t));

    dst->byte.SOF  = _htons(PROTOCOL_SOF);
    dst->byte.Type = (uint8_t)FrameType_Ack;
    dst->byte.Len  = 4; // Ack固定4字节
    dst->byte.Ack  = _htonl(Ack); // 注意32位用htonl

    // 校验范围: Type, Len, Ack 共 1 + 1 + 4 = 6 字节
    uint16_t crc_val = CRC16(&dst->data[2], 10 - 4);
    dst->byte.CRC = _htons(crc_val);

    return true;
}

bool Portocol_UnPackAckFrame(Protocol_AckFrame_t* dst, uint8_t* data, uint8_t data_len)
{
    if (dst == NULL || data == NULL || data_len != sizeof(Protocol_AckFrame_t))
        return false;

    memcpy(dst->data, data, data_len);

    if (_ntohs(dst->byte.SOF) != PROTOCOL_SOF || dst->byte.Type != FrameType_Ack)
        return false;

    uint16_t crc_calc = CRC16(&dst->data[2], sizeof(Protocol_AckFrame_t) - 4);
    if (crc_calc != _ntohs(dst->byte.CRC))
        return false;

    // 【赋值还原】
#ifdef PROTOCOL_HTONS
    dst->byte.SOF = _ntohs(dst->byte.SOF);
    dst->byte.Ack = _ntohl(dst->byte.Ack); // 32位用ntohl
    dst->byte.CRC = _ntohs(dst->byte.CRC);
#endif

    return true;
}