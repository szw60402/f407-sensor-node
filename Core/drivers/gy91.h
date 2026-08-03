/**
 * GY-91 9轴姿态传感器驱动 (I²C)  -- 板上芯片 = ICM-20948
 *
 * I²C 地址: 0x68 (AD0 = GND)
 * 输出: 3轴加速度(mg), 3轴陀螺仪(dps ×10)
 * Bank 切换: 不同寄存器分布在不同 Bank, 读写前需先切 Bank
 */

#ifndef GY91_H
#define GY91_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define GY91_I2C_ADDR  0x68  /* AD0=GND; 实验记录: 0x68/0x69 均无应答(2026-07-31), 问题在模块/线 */

/**
 * 加速度满量程; 可选: 2g, 4g, 8g, 16g
 */
typedef enum {
    GY91_ACCEL_2G  = 0x00,
    GY91_ACCEL_4G  = 0x01,
    GY91_ACCEL_8G  = 0x02,
    GY91_ACCEL_16G = 0x03
} icm_accel_range_t;

/**
 * 陀螺仪满量程; 可选: 250/500/1000/2000 dps
 */
typedef enum {
    GY91_GYRO_250  = 0x00,
    GY91_GYRO_500  = 0x01,
    GY91_GYRO_1000 = 0x02,
    GY91_GYRO_2000 = 0x03
} icm_gyro_range_t;

/* ====== 初始化 ======
 * 唤醒 + 配置加速度量程 ±2g、陀螺仪量程 ±250dps */
HAL_StatusTypeDef gy91_init(I2C_HandleTypeDef *hi2c);

/* ====== 读取数据 ======
 * 一次读回全部 6 轴原始值 (加速度 mg, 陀螺仪 dps×10) */
HAL_StatusTypeDef gy91_read(I2C_HandleTypeDef *hi2c,
                                 int16_t *accel_x, int16_t *accel_y, int16_t *accel_z,
                                 int16_t *gyro_x,  int16_t *gyro_y,  int16_t *gyro_z);

#endif
