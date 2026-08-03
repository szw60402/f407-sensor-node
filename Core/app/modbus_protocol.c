/**
 * @file  modbus_protocol.c
 * @brief Modbus RTU 协议处理 —— 整个 Modbus 通信的核心
 *
 * 支持的 Modbus 功能码:
 *   - 0x03: 读保持寄存器 (Read Holding Registers)
 *   - 0x06: 写单寄存器 (Write Single Register)
 *
 * 寄存器映射表 (从站地址固定为 0x01):
 *   ┌─────────┬──────────────┬─────────────┐
 *   │ 地址     │  含义         │  数据类型    │
 *   ├─────────┼──────────────┼─────────────┤
 *   │ 0x0000  │  温度 (×10)   │  int16_t    │
 *   │ 0x0001  │  湿度 (×10)   │  uint16_t   │
 *   │ 0x0002  │  加速度 X     │  int16_t    │
 *   │ 0x0003  │  加速度 Y     │  int16_t    │
 *   │ 0x0004  │  加速度 Z     │  int16_t    │
 *   │ 0x0005  │  电流 (mA)    │  int16_t    │
 *   │ 0x0006  │  光照 (0-100) │  uint16_t   │
 *   └─────────┴──────────────┴─────────────┘
 *
 * Modbus RTU 帧格式 (以 03 功能码为例):
 *   主机请求: [地址][03][起始寄存器H][起始寄存器L][数量H][数量L][CRCL][CRCH]
 *   从机应答: [地址][03][字节数][数据H][数据L]...[CRCL][CRCH]
 */

#include "modbus_protocol.h"
#include "app_global.h"
#include "utils/crc16.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>

/* ====== 从全局结构体读单个寄存器的值 ======
 * 这里用"抢锁读 → 读完马上放"的方式, 而不是在 modbus_process 外面加锁,
 * 因为 ModbusTask 只在需要时短暂读, 不会长时间占用 Mutex。 */
static uint16_t reg_read(uint16_t addr)
{
    uint16_t val = 0;

    /* 拿锁 —— 用 0 超时 = 拿不到立刻放弃 (不等), 防止因为 SensorTask
     * 正在写数据而导致 Modbus 无法回复 (丢帧比卡死强) */
    if (xSemaphoreTake(sensorDataMutexHandle, 0) == pdTRUE) {
        switch (addr) {
            case 0x0000: val = (int16_t)(g_sensor_data.temperature_c * 10.0); break;
            case 0x0001: val = (uint16_t)(g_sensor_data.humidity_pct * 10.0); break;
            case 0x0002: val = (uint16_t)g_sensor_data.accel_x; break;
            case 0x0003: val = (uint16_t)g_sensor_data.accel_y; break;
            case 0x0004: val = (uint16_t)g_sensor_data.accel_z; break;
            case 0x0005: val = (int16_t)(g_sensor_data.current_a * 1000.0); break;
            case 0x0006: val = g_sensor_data.light_pct; break;
            default: val = 0x0000; break;
        }
        xSemaphoreGive(sensorDataMutexHandle);
    }
    return val;
}

/* 写寄存器回调 —— 项目A 不写 Flash, 这个函数以后可以扩展 */
static void reg_write_cb(uint16_t addr, uint16_t val)
{
    (void)addr; (void)val;
    /* 预留: 以后可以在这写 Modbus 配置参数、校准值 */
}

/* Modbus 异常码 */
#define MB_EX_ILLEGAL_FUNC   0x01   /* 不支持的功能码 */
#define MB_EX_ILLEGAL_ADDR   0x02   /* 寄存器地址不存在 */
#define MB_EX_ILLEGAL_VALUE  0x03   /* 数据值非法 */

/**
 * @brief 构建异常应答帧
 *
 * 格式: [地址][功能码|0x80][异常码][CRCL][CRCH]
 *   功能码最高位置 1 = 告诉主机 "你请求的功能出错了"
 */
static uint16_t build_exception(uint8_t *tx, uint8_t func, uint8_t code)
{
    tx[0] = MODBUS_SLAVE_ADDR;
    tx[1] = func | 0x80;        /* 功能码 bit7=1 表示异常 */
    tx[2] = code;
    uint16_t crc = crc16_calc(tx, 3);
    tx[3] = crc & 0xFF;          /* CRC 低字节在前 (Modbus 规定) */
    tx[4] = (crc >> 8) & 0xFF;
    return 5;                     /* 异常应答固定 5 字节 */
}

/**
 * @brief 处理 Modbus 请求帧, 生成应答帧
 * @return 应答帧字节数, 0 = 不回复
 */
uint16_t modbus_process(const uint8_t *rx_buf, uint16_t rx_len,
                         uint8_t *tx_buf, uint16_t tx_max)
{
    /* 最少 4 字节: 地址 + 功能码 + CRC */
    if (rx_len < 4 || tx_max < 8) return 0;

    /* 地址不匹配 → 不是发给我的, 不回复 (Modbus 标准行为) */
    if (rx_buf[0] != MODBUS_SLAVE_ADDR) return 0;

    /* CRC 校验失败 → 传输过程中有比特翻转, 不回复 */
    if (!crc16_verify(rx_buf, rx_len)) return 0;

    uint8_t func = rx_buf[1];

    switch (func) {

    /* ========== 03: 读保持寄存器 ==========
     * 请求: [addr][03][起始H][起始L][数量H][数量L][CRCL][CRCH]
     *   固定 8 字节
     * 应答: [addr][03][字节数][数据H][数据L]...[CRCL][CRCH] */
    case 0x03:
    {
        if (rx_len != 8)
            return build_exception(tx_buf, func, MB_EX_ILLEGAL_VALUE);

        uint16_t start_addr = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
        uint16_t quantity   = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

        if (quantity < 1 || quantity > 8)  /* 一次最多 8 个寄存器 */
            return build_exception(tx_buf, func, MB_EX_ILLEGAL_VALUE);

        if (start_addr + quantity > 7)      /* 超出我们定义的 8 个寄存器 */
            return build_exception(tx_buf, func, MB_EX_ILLEGAL_ADDR);

        uint8_t byte_count = quantity * 2;  /* 每个寄存器 = 2 字节 */

        tx_buf[0] = MODBUS_SLAVE_ADDR;
        tx_buf[1] = func;
        tx_buf[2] = byte_count;

        /* 逐寄存器读值 → 大端序写入应答 */
        for (uint16_t i = 0; i < quantity; i++) {
            uint16_t val = reg_read(start_addr + i);
            tx_buf[3 + i * 2]     = (val >> 8) & 0xFF;
            tx_buf[3 + i * 2 + 1] = val & 0xFF;
        }

        /* 计算 CRC 附加到末尾 */
        uint16_t resp_len = 3 + byte_count;
        uint16_t crc = crc16_calc(tx_buf, resp_len);
        tx_buf[resp_len]     = crc & 0xFF;
        tx_buf[resp_len + 1] = (crc >> 8) & 0xFF;
        return resp_len + 2;
    }

    /* ========== 06: 写单寄存器 ==========
     * 请求: [addr][06][寄存器H][寄存器L][值H][值L][CRCL][CRCH]
     * 应答: 回显请求 (Modbus 标准行为) */
    case 0x06:
    {
        if (rx_len != 8)
            return build_exception(tx_buf, func, MB_EX_ILLEGAL_VALUE);

        uint16_t addr = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
        uint16_t val  = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

        if (addr > 7)
            return build_exception(tx_buf, func, MB_EX_ILLEGAL_ADDR);

        reg_write_cb(addr, val);

        /* 应答 = 原样返回请求的前 6 字节 + 重新算 CRC */
        memcpy(tx_buf, rx_buf, 6);
        uint16_t crc = crc16_calc(tx_buf, 6);
        tx_buf[6] = crc & 0xFF;
        tx_buf[7] = (crc >> 8) & 0xFF;
        return 8;
    }

    default:
        return build_exception(tx_buf, func, MB_EX_ILLEGAL_FUNC);
    }
}
