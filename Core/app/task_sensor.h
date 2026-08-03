/**
 * @file  task_sensor.h
 * @brief SensorTask: 100ms 周期采集全部传感器
 *
 * 采集: SHT3X, ICM-20948, ACS712, GL5516
 * 写入: g_sensor_data (Mutex 保护)
 */

#ifndef TASK_SENSOR_H
#define TASK_SENSOR_H

void SensorTaskFunc(void *argument);

#endif
