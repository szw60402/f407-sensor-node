/**
 * @file  task_sensor.c
 * @brief SensorTask —— 每 100ms 采集一次全部传感器数据
 *
 * 采集顺序:
 *   1. SHT3X (温湿度, I²C) —— 最慢(~20ms), 先跑
 *   2. GY-91 模块 (加速度+陀螺仪, 实测芯片=MPU-6500, 假 GY-91)
 *   3. ACS712 (电流, ADC PA0) —— DMA 后台在填, 读缓冲区即可
 *   4. GL5516 (光照, ADC PA1) —— 同上
 *
 * 写全局数据时必须拿 Mutex:
 *   xSemaphoreTake(sensorDataMutexHandle) → 写入 g_sensor_data → xSemaphoreGive
 *   这样 ModbusTask/CANTask/DisplayTask 读的时候不会被写一半的脏数据坑到。
 */

#include "task_sensor.h"
#include "app_global.h"
#include "drivers/sht3x.h"
#include "drivers/gy91.h"
#include "drivers/acs712.h"
#include "drivers/gl5516.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "adc.h"
#include "i2c.h"

/* ---------- 外部变量 ---------- */
extern volatile uint32_t adc_dma_buf[2];    /* ADC DMA 双通道缓冲区:
                                               [0]=PA0(ACS712), [1]=PA1(GL5516) */
/* sensorDataMutexHandle 已在 app_global.h 中声明 */

sensor_data_t g_sensor_data;   /* 全局传感器数据 (定义在这, 声明在 app_global.h) */

void SensorTaskFunc(void *argument)
{
    (void)argument;

    /* 初始化 USB CDC (虚拟串口) —— 让 printf 能从 USB 口出来 */
    extern void MX_USB_DEVICE_Init(void);
    MX_USB_DEVICE_Init();

    /* ----- 启动 ADC DMA 循环采集 -----
     * 硬件在后台不停地把 PA0/PA1 的采样值写到 adc_dma_buf[]。
     * 我们只需要在任务循环里读这个数组, 不用调 HAL_ADC_GetValue()。
     * 参数: &hadc1=ADC1句柄, buf=目标缓冲区地址, 2=通道数 */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, 2);

    /* 开 ADC 连续转换: DMA 循环需要每次转换都持续产生数据。
     * CubeMX 里没开 Continuous Conversion, 默认跑完一轮就停,
     * 缓冲只会被写入一次快照, 之后永远不更新。 */
    hadc1.Instance->CR2 |= ADC_CR2_CONT;

    /* 等传感器上电稳定 (ACS712 OUT 需要 ~100ms 爬升到 VCC/2) */
    osDelay(200);

    /* ====== ACS712 零点校准 ======
     * 上电时负载未接, 采样 32 次取平均得到真实零点 (VCC/2),
     * 补偿模块间供电差异导致的 ±0.5A 假偏移。 */
    uint32_t zero_sum = 0;
    for (uint8_t i = 0; i < 32; i++) {
        zero_sum += adc_dma_buf[0] & 0xFFFF;   /* 低16位 = PA0 = ACS712 */
        osDelay(5);
    }
    acs712_calibrate_zero(&hadc1, zero_sum / 32);

    /* ====== 初始化 GY-91 模块 (唤醒 + 配置量程) ======
     * 之前只调了 gy91_read 没调 init, 芯片一直处于休眠模式,
     * 读回的全是垃圾值 (0xFFFF)。init 内有 15ms 延时, 放任务里安全。
     * 拿 I2C 锁: 防止与 DisplayTask 的 OLED 刷新抢总线。 */
    if (osMutexAcquire(i2c1MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
        gy91_init(&hi2c1);
        osMutexRelease(i2c1MutexHandle);
    }

    for (;;) {

        /* ====== 拿锁: 写期间禁止任何人读全局数据 ====== */
        if (xSemaphoreTake(sensorDataMutexHandle, pdMS_TO_TICKS(500)) == pdTRUE) {

            /* ====== I2C 总线锁: 读传感器时禁止 DisplayTask 刷新 OLED ======
             * 不保护的话 SensorTask 可能在 DisplayTask 发 OLED 像素途中抢占,
             * 导致 HAL I2C 状态机被打乱, OLED 花屏。 */
            if (osMutexAcquire(i2c1MutexHandle, pdMS_TO_TICKS(100)) == osOK) {

                /* ----- SHT3X 温湿度 ----- */
                HAL_StatusTypeDef sht_ret = sht3x_read(&hi2c1,
                    &g_sensor_data.temperature_c, &g_sensor_data.humidity_pct);
                g_sensor_data.sht3x_error = (sht_ret != HAL_OK) ? 1 : 0;

                /* ----- GY-91 姿态 ----- */
                HAL_StatusTypeDef gy91_ret = gy91_read(&hi2c1,
                    &g_sensor_data.accel_x, &g_sensor_data.accel_y, &g_sensor_data.accel_z,
                    &g_sensor_data.gyro_x,  &g_sensor_data.gyro_y,  &g_sensor_data.gyro_z);
                g_sensor_data.gy91_error = (gy91_ret != HAL_OK) ? 1 : 0;

                osMutexRelease(i2c1MutexHandle);
            }

            /* ----- ACS712 电流 (PA0 = 转换序列第1项) -----
             * DMA 半字模式: 两次转换打包在 adc_dma_buf[0] 里,
             * 低16位=第1次转换(CH0/PA0), 高16位=第2次转换(CH1/PA1)。
             * 不能把整个 32 位当 ADC 值, 否则数值放大 6 万倍。 */
            g_sensor_data.current_a = acs712_read_current(&hadc1, adc_dma_buf[0] & 0xFFFF);

            /* ----- GL5516 光照 (PA1 = 转换序列第2项) ----- */
            g_sensor_data.light_pct = gl5516_light_pct((adc_dma_buf[0] >> 16) & 0xFFFF);

            /* 时间戳 (ms 级, 用于调试) */
            g_sensor_data.timestamp_ms = osKernelGetSysTimerCount();
            g_sensor_data.adc_error = 0;

            /* ====== 放锁: 写入完毕, 其他任务可以读了 ====== */
            xSemaphoreGive(sensorDataMutexHandle);
        }

        /* 精确 100ms 周期: 挂起自己, 让出 CPU 给其他任务 */
        osDelay(100);
    }
}
