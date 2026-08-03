/**
 * @file  task_modbus.h
 * @brief ModbusTask: 阻塞等待 USART3 IDLE 信号量 → 解析帧 → 回包
 */

#ifndef TASK_MODBUS_H
#define TASK_MODBUS_H

void ModbusTaskFunc(void *argument);

#endif
