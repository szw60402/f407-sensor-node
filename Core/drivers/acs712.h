/**
 * @file  acs712.h
 * @brief ACS712-05B 电流传感器驱动 (ADC)
 *
 * 供电: 5V, 零点: VCC/2=2.5V, 灵敏度: 185mV/A
 * 分压: 13kΩ + 27kΩ, 分压比 0.675
 * ADC 通道: PA0 (ADC1_IN0)
 */

#ifndef ACS712_H
#define ACS712_H

#include "stm32f4xx_hal.h"

#define ACS712_ZERO_V    2.50    /* 零点电压(5V供电) */
#define ACS712_SENS_V_A  0.185   /* 灵敏度 185mV/A */
#define ACS712_DIV_RATIO 0.675   /* 分压比 27/(13+27) */

/**
 * @brief 读取电流值
 * @param hadc ADC 句柄
 * @param adc_val 输入: ADC 原始值 (0-4095)
 * @return 电流(A), 正值 = 正向, 负值 = 反向
 */
double acs712_read_current(ADC_HandleTypeDef *hadc, uint32_t adc_val);
void  acs712_calibrate_zero(ADC_HandleTypeDef *hadc, uint32_t adc_val);

#endif
