/**
 * @file  tja1050.h
 * @brief TJA1050 CAN 收发器 + CAN 帧发送封装
 *
 * CAN1 配置: 500kbps, PD0(RX)/PD1(TX), AF9
 */

#ifndef TJA1050_H
#define TJA1050_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/**
 * @brief 发送标准 CAN 帧
 * @param hcan    CAN 句柄 (hcan1)
 * @param id      CAN ID (11-bit)
 * @param data    8 字节数据
 * @param len     数据长度 (0-8)
 * @return HAL_OK 成功
 */
HAL_StatusTypeDef can_send_frame(CAN_HandleTypeDef *hcan,
                                  uint32_t id, const uint8_t *data, uint8_t len);

/**
 * @brief 组装并广播传感器状态帧 (ID=0x100)
 * @param hcan     CAN 句柄
 * @param temp_x10 温度 ×10 (如 256 = 25.6°C)
 * @param hum_x10  湿度 ×10
 * @param current_ma 电流 mA
 * @param light_pct 光照 0-100
 */
HAL_StatusTypeDef can_send_sensor_frame(CAN_HandleTypeDef *hcan,
                                         int16_t temp_x10, uint16_t hum_x10,
                                         int16_t current_ma, uint8_t light_pct);

#endif
