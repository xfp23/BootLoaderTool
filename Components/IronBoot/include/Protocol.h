#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ---------------- Protocol Config ---------------- */

#define PROTOCOL_DATAFRAME_BINLEN 8 // 每帧数据帧携带的最大字节长度

// 计算总包长: SOF(2) + Type(1) + Len(2) + Seq(2) + Bin(8) + CRC(2) = 17
#define PROTOCOL_DATAFRAME_SIZE (2 + 1 + 2 + 2 + PROTOCOL_DATAFRAME_BINLEN + 2)


#if defined(_MSC_VER)
#define PACKED_STRUCT_START __pragma(pack(push, 1))
#define PACKED_STRUCT_END __pragma(pack(pop))
#define PACKED_ATTR
#define PROTOCOL_HTONS
#elif defined(__GNUC__) || defined(__clang__)
#define PACKED_STRUCT_START
#define PACKED_STRUCT_END
#define PACKED_ATTR __attribute__((packed))
#else
#define PACKED_STRUCT_START
#define PACKED_STRUCT_END
#define PACKED_ATTR
#endif

/* ---------------- 字节序转换宏 ---------------- */
#ifdef PROTOCOL_HTONS
    // 小端转大端 (Host to Network Short/Long)
#define _htons(x) (uint16_t)((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
#define _ntohs(x) _htons(x)
#define _htonl(x) (uint32_t)((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
                                 (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#define _ntohl(x) _htonl(x)
#else
    // 不转换，直接返回
#define _htons(x) (x)
#define _ntohs(x) (x)
#define _htonl(x) (x)
#define _ntohl(x) (x)
#endif

/* ---------------- 核心协议结构体定义 ---------------- */

PACKED_STRUCT_START

/**
 * @brief 数据帧格式 (PROTOCOL_DATAFRAME_SIZE字节)
 */
typedef union
{
    struct PACKED_ATTR
    {
        uint16_t SOF; /* 帧头 0xAA 0x55 */
        uint8_t Type; /* 帧类型 */
        uint16_t Len; /* 数据有效长度 (不包含头尾) */
        uint16_t Seq; /* 帧序号 */
        uint8_t Bin[PROTOCOL_DATAFRAME_BINLEN];
        uint16_t CRC; /* CRC16 */
    } byte;
    uint8_t data[PROTOCOL_DATAFRAME_SIZE];
} Protocol_DataFrame_t;

/**
 * @brief 命令帧格式 (8 字节)
 */
typedef union
{
    struct PACKED_ATTR
    {
        uint16_t SOF;
        uint8_t Type;
        uint8_t Len;
        uint16_t Cmd; /* 2字节命令 */
        uint16_t CRC;
    } byte;
    uint8_t data[8];
} Protocol_CmdFrame_t;

/**
 * @brief 应答帧格式 (10 字节)
 * 关键点：Ack 统一为 uint32_t，兼容 CRC16 和 CRC32
 */
typedef union
{
    struct PACKED_ATTR
    {
        uint16_t SOF;
        uint8_t Type;
        uint8_t Len;
        uint32_t Ack; /* 应答值（CRC16/CRC32） */
        uint16_t CRC; /* 本帧校验 */
    } byte;
    uint8_t data[10];
} Protocol_AckFrame_t;

PACKED_STRUCT_END

/* ---------------- 枚举定义 ---------------- */

typedef enum
{
    FrameType_Cmd = 0,
    FrameType_Data = 1,
    FrameType_Ack = 2,
} Protocol_FrameType_t;

typedef enum
{
    PROTOCOL_CMD_RESET = 0xACED,
    PROTOCOL_CMD_STARTBL = 0xC0D5,
    PROTOCOL_CMD_EARSE = 0x23DC,
    PROTOCOL_CMD_BIN = 0xAD8E,
    PROTOCOL_CMD_COMP = 0xBD58,
    PROTOCOL_CMD_CHECKBIN = 0xDEAE,
    PROTOCOL_CMD_SUCC = 0x356E,
    PROTOCOL_CMD_FAIL = 0x2544,

    PROTOCOL_ACK_REFUSE = 0x650D, // 通用拒绝
} Protocol_CmdAck_t;

/* ---------------- 函数原型声明 ---------------- */

bool Portocol_PackDataFrame(Protocol_DataFrame_t *dst, uint16_t seq, uint8_t *data, uint16_t data_len);
bool Portocol_UnPackDataFrame(Protocol_DataFrame_t *dst, uint8_t *data, size_t len);

bool Portocol_PackCmdFrame(Protocol_CmdFrame_t *dst, Protocol_CmdAck_t cmd);
bool Portocol_UnPackCmdFrame(Protocol_CmdFrame_t *dst, uint8_t *data, size_t data_len);

bool Portocol_PackAckFrame(Protocol_AckFrame_t *dst, uint32_t Ack);
bool Portocol_UnPackAckFrame(Protocol_AckFrame_t *dst, uint8_t *data, uint8_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */