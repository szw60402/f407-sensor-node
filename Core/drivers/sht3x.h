/**
 * @file  sht3x.h
 * @brief SHT3X-DIS 温湿度传感器驱动 (I²C)
 *
 * I²C 地址: 0x44
 * 精度: 温度 ±0.3°C, 湿度 ±2%RH
 * 参考: Sensirion SHT3X-DIS Datasheet
 */

#ifndef SHT3X_H
#define SHT3X_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SHT3X_I2C_ADDR  0x44  /* 7-bit 地址 (ADDR 引脚低电平) */

/**
 * @brief 读取一次温湿度 (单次测量 + 时钟 stretching 使能)
 * @param hi2c    I2C 句柄 (hi2c1)
 * @param temp_c  输出: 温度(°C)
 * @param hum_pct 输出: 相对湿度(%RH)
 * @return HAL_OK 成功, 其他值失败
 */
HAL_StatusTypeDef sht3x_read(I2C_HandleTypeDef *hi2c,
                              double *temp_c, double *hum_pct);

#endif
