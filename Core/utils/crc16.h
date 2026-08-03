/**
 * @file  crc16.h
 * @brief Modbus CRC-16 查表实现
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/**
 * @brief 计算 Modbus RTU CRC-16
 * @param buf  数据缓冲区
 * @param len  字节数
 * @return 16 位 CRC（低字节在前，符合 Modbus 规范）
 */
uint16_t crc16_calc(const uint8_t *buf, uint16_t len);

/**
 * @brief 验证 Modbus 帧 CRC
 * @param buf  完整帧（包含尾部 CRC）
 * @param len  帧总长度（数据+CRC=2字节）
 * @return 1=校验通过, 0=校验失败
 */
uint8_t crc16_verify(const uint8_t *buf, uint16_t len);

#endif
