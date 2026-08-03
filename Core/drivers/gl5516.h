/**
 * @file  gl5516.h
 * @brief GL5516 光敏电阻驱动 (ADC 分压)
 *
 * 电路: 3.3V -> GL5516 -> PA1(ADC1_IN1) -> R9(10kΩ) -> GND
 * ADC 通道: PA1
 */

#ifndef GL5516_H
#define GL5516_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/**
 * @brief 从 ADC 值计算 GL5516 分压后的电压
 * @param adc_val ADC 原始值 (0-4095)
 * @return 电压(V), 0~3.3V 范围
 */
double gl5516_read_voltage(uint32_t adc_val);

/**
 * @brief 估算光照相对强度 (0-100%)
 * @note  GL5516 离散性大, 值为相对参考, 不追求绝对照度
 * @param adc_val ADC 原始值
 * @return 0=暗, 100=亮
 */
uint8_t gl5516_light_pct(uint32_t adc_val);

#endif
