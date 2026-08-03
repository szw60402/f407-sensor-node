/**
 * @file  app_global.h
 * @brief 全局传感器数据结构体 + Mutex 句柄声明
 *
 * SensorTask 写, ModbusTask/CANTask/DisplayTask 读.
 * 读/写前必须 xSemaphoreTake(sensorDataMutexHandle, portMAX_DELAY).
 */

#ifndef APP_GLOBAL_H
#define APP_GLOBAL_H

#include "cmsis_os.h"
#include <stdint.h>

/* ---------- 传感器数据 ---------- */
typedef struct {
    /* 温湿度 */
    double  temperature_c;       /* °C */
    double  humidity_pct;        /* %RH */

    /* 姿态 (整数 ×10) */
    int16_t accel_x;  int16_t accel_y;  int16_t accel_z; /* mg */
    int16_t gyro_x;   int16_t gyro_y;   int16_t gyro_z;  /* dps×10 */

    /* 电流 */
    double  current_a;           /* A */

    /* 光照 */
    uint8_t light_pct;           /* 0-100 */

    /* 采集时间戳 */
    uint32_t timestamp_ms;

    /* 错误标志 */
    uint8_t sht3x_error : 1;
    uint8_t gy91_error  : 1;
    uint8_t adc_error  : 1;
    uint8_t reserved   : 5;
} sensor_data_t;

/* ---------- 全局实例 + Mutex ---------- */
extern sensor_data_t g_sensor_data;
extern osMutexId_t   sensorDataMutexHandle;
extern osMutexId_t   i2c1MutexHandle;   /* 保护 I2C1 总线 (SensorTask + DisplayTask 共用) */

#endif
