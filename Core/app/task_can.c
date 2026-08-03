/**
 * @file  task_can.c
 * @brief CANTask —— 每秒通过 CAN 总线广播一次传感器数据
 *
 * CAN 帧格式 (Standard ID = 0x100):
 *   ├── Byte 0-1: 温度 (int16 ×10, 大端)
 *   ├── Byte 2-3: 湿度 (uint16 ×10, 大端)
 *   ├── Byte 4-5: 电流 (int16, mA, 大端)
 *   ├── Byte 6:   光照 (uint8, 0-100)
 *   └── Byte 7:   保留
 *
 * 单节点测试时 ACK 错误是正常的 —— CAN 总线上至少需要 2 个节点
 * 才能完成 ACK 握手。接上 USB-CAN 工具或第二个开发板后错误消失。
 */

#include "task_can.h"
#include "app_global.h"
#include "drivers/tja1050.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "can.h"

/* 全局数据锁 (在 freertos.c 中创建) */
/* sensorDataMutexHandle 已在 app_global.h 中声明 */

void CANTaskFunc(void *argument)
{
    (void)argument;

    /* 启动 CAN1 外设 */
    HAL_CAN_Start(&hcan1);

    for (;;) {

        /* 读全局数据 (和 SensorTask 竞争, 拿锁读) */
        if (xSemaphoreTake(sensorDataMutexHandle, pdMS_TO_TICKS(200)) == pdTRUE) {

            /* 浮点数 → 整数 (CAN 协议不传浮点数, 用 ×10/×1000 整形化) */
            int16_t temp_x10 = (int16_t)(g_sensor_data.temperature_c * 10.0);
            uint16_t hum_x10 = (uint16_t)(g_sensor_data.humidity_pct * 10.0);
            int16_t cur_ma   = (int16_t)(g_sensor_data.current_a * 1000.0);

            /* 组装并发送 CAN 帧 (具体实现在 tja1050.c) */
            can_send_sensor_frame(&hcan1, temp_x10, hum_x10,
                                  cur_ma, g_sensor_data.light_pct);

            /* 读完放锁 */
            xSemaphoreGive(sensorDataMutexHandle);
        }

        /* 1 秒广播一次 —— 工业传感器节点常见的上报周期 */
        osDelay(1000);
    }
}
