/**
 * @file  modbus_protocol.h
 * @brief Modbus RTU 协议层 (03/06 功能码)
 */

#ifndef MODBUS_PROTOCOL_H
#define MODBUS_PROTOCOL_H

#include <stdint.h>

#define MODBUS_SLAVE_ADDR  0x01

/**
 * @brief 处理 Modbus 请求帧, 生成应答帧
 * @param rx_buf  接收到的 Modbus 帧 (含 CRC)
 * @param rx_len  帧长度
 * @param tx_buf  输出: 应答帧缓冲区
 * @param tx_max  tx_buf 最大长度
 * @return 应答帧长度, 0 = 不回复 (广播/CRC错/地址不匹配)
 */
uint16_t modbus_process(const uint8_t *rx_buf, uint16_t rx_len,
                         uint8_t *tx_buf, uint16_t tx_max);

#endif
