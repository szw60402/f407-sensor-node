/**
 * @file  sht3x.c
 * @brief SHT3X-DIS 温湿度传感器驱动实现
 *
 * 通信流程:
 *   1. 主机发 2 字节测量命令 (0x2C06 = 高精度模式)
 *   2. 等 20ms 让传感器完成采样
 *   3. 读回 6 字节: 温度高8位、温度低8位、温度CRC、
 *                   湿度高8位、湿度低8位、湿度CRC
 *   4. 套公式把原始值转为 °C 和 %RH
 *
 * 温度公式 (Datasheet 第 4.13 节):
 *   T[°C] = -45 + 175 × (原始值 / 65535)
 * 湿度公式:
 *   RH[%] = 100 × (原始值 / 65535)
 */

#include "sht3x.h"

/* 向传感器发送的测量命令:
 *   0x2C06 = 高精度测量 + 时钟 stretching 使能
 *   时钟 stretching = 传感器没测完时拉住 SCL 线不放, 主机自动等 */
#define SHT3X_CMD_MEASURE_HIGH    0x2C06

/* 高精度模式需要约 15ms 完成采样, 这里留到 20ms 保险 */
#define SHT3X_MEASURE_DELAY_MS    20

HAL_StatusTypeDef sht3x_read(I2C_HandleTypeDef *hi2c,
                              double *temp_c, double *hum_pct)
{
    uint8_t cmd[2];     /* 2 字节命令 */
    uint8_t data[6];    /* 传感器返回 6 字节 */
    uint16_t raw_temp, raw_hum;
    HAL_StatusTypeDef ret;

    /* -------- 第1步: 发测量命令 ----------
     * I²C 地址左移 1 位是因为 HAL 库要求地址对齐到 bit7,
     * 而我们存的 0x44 是 7 位地址 (bit6..bit0) */
    cmd[0] = (SHT3X_CMD_MEASURE_HIGH >> 8) & 0xFF;  /* 0x2C */
    cmd[1] = SHT3X_CMD_MEASURE_HIGH & 0xFF;         /* 0x06 */
    ret = HAL_I2C_Master_Transmit(hi2c, SHT3X_I2C_ADDR << 1, cmd, 2, 100);
    if (ret != HAL_OK) return ret;  /* 总线不通 → 直接返回错误码 */

    /* -------- 第2步: 等采样完成 ----------
     * 这个延时必须放, 否则读回来的数据可能是旧值 */
    HAL_Delay(SHT3X_MEASURE_DELAY_MS);

    /* -------- 第3步: 读 6 字节 ----------
     * 字节0-1 = 温度原始值 (MSB在前)
     * 字节2   = 温度 CRC (这里不做校验, 简化处理)
     * 字节3-4 = 湿度原始值
     * 字节5   = 湿度 CRC */
    ret = HAL_I2C_Master_Receive(hi2c, SHT3X_I2C_ADDR << 1, data, 6, 100);
    if (ret != HAL_OK) return ret;

    /* -------- 第4步: 拼原始值 ----------
     * 芯片用大端序 (高位在前), 把两个字节拼成 16 位整数 */
    raw_temp = ((uint16_t)data[0] << 8) | data[1];
    raw_hum  = ((uint16_t)data[3] << 8) | data[4];

    /* -------- 第5步: 转物理量 ----------
     * 公式来自官方 Datasheet:
     *   温度: -45 + 175 × 原始值 ÷ 65535
     *   湿度: 100 × 原始值 ÷ 65535
     * 为什么是 65535? 因为传感器用 16 位 ADC, 最大输出值 = 2^16-1 */
    *temp_c  = -45.0 + 175.0 * ((double)raw_temp / 65535.0);
    *hum_pct = 100.0 * ((double)raw_hum / 65535.0);

    return HAL_OK;
}
