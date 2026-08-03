/**
 * @file  task_display.c
 * @brief DisplayTask —— 每 500ms 刷新 OLED 屏幕
 *
 * 显示布局 (128×64, 5x7 字体, 每行 ~21 字符):
 *
 *   行 0 (Page 0): T:25.6C H:45%     ← 温湿度
 *   行 1 (Page 2): X: 1024 Y: -512   ← 加速度 X/Y
 *   行 2 (Page 4): I:0.012A           ← 电流
 *   行 3 (Page 6): L: 85%             ← 光照
 *
 * 用 Page 0/2/4/6 而不连续, 是因为双行间距看起来更舒服。
 */

#include "task_display.h"
#include "app_global.h"
#include "drivers/ssd1306.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "i2c.h"
#include <stdio.h>

/* sensorDataMutexHandle 已在 app_global.h 中声明 */

volatile uint32_t dbg_display_tick = 0;   /* 诊断: DisplayTask 心跳, 每轮循环 +1 */

void DisplayTaskFunc(void *argument)
{
    (void)argument;

    /* 等 I²C 总线上电稳定 + 等 SensorTask 至少跑完一轮 */
    osDelay(200);

    /* 初始化 OLED (只初始化一次, 以后只刷新数据) */
    ssd1306_init(&hi2c1);

    char line[21];  /* 每行缓冲区: 128÷6≈21 字符 */

    for (;;) {
        dbg_display_tick++;   /* 诊断心跳 */

        /* 抢锁读数据 (200ms 超时 —— DisplayTask 优先级最低,
         * 拿不到就跳过这轮, 反正 500ms 后还有一轮) */
        if (xSemaphoreTake(sensorDataMutexHandle, pdMS_TO_TICKS(200)) == pdTRUE) {

            /* ====== LED 状态灯 (面包板上的 PD12/PD13 指示灯) ======
             * 绿灯: 心跳 —— 500ms 翻转一次, 闪 = 程序活着
             * 红灯: 任一传感器错误即点亮, 一眼看出谁挂了 */
            HAL_GPIO_TogglePin(GPIOD, LED_GREEN_Pin);
            if (g_sensor_data.sht3x_error || g_sensor_data.gy91_error || g_sensor_data.adc_error)
                HAL_GPIO_WritePin(GPIOD, LED_RED_Pin, GPIO_PIN_SET);
            else
                HAL_GPIO_WritePin(GPIOD, LED_RED_Pin, GPIO_PIN_RESET);

            /* ===== 左列 4 行 (页 0/2/4/6) ===== */
            snprintf(line, sizeof(line), "T:%5.1fC", g_sensor_data.temperature_c);
            ssd1306_clear_area(0, 0, 128);
            ssd1306_puts(0, 0, line);

            snprintf(line, sizeof(line), "H:%5.0f%%", g_sensor_data.humidity_pct);
            ssd1306_clear_area(2, 0, 128);
            ssd1306_puts(2, 0, line);

            snprintf(line, sizeof(line), "I:%8.3fA", g_sensor_data.current_a);
            ssd1306_clear_area(4, 0, 128);
            ssd1306_puts(4, 0, line);

            snprintf(line, sizeof(line), "L:%3d%%", g_sensor_data.light_pct);
            ssd1306_clear_area(6, 0, 128);
            ssd1306_puts(6, 0, line);

            /* ===== 右列 XYZ (页 1/3/5 = 左列行间距的空隙页) =====
             * 妙处: 不同页互不冲突, XYZ 整体比左列低 8px, 视觉居中
             * 列 78 起 (13 字符 x 6px), 左边四行不动 */
            snprintf(line, sizeof(line), "X:%5.2f", g_sensor_data.accel_x / 16384.0);
            ssd1306_clear_area(1, 0, 128);
            ssd1306_puts(1, 78, line);

            snprintf(line, sizeof(line), "Y:%5.2f", g_sensor_data.accel_y / 16384.0);
            ssd1306_clear_area(3, 0, 128);
            ssd1306_puts(3, 78, line);

            snprintf(line, sizeof(line), "Z:%5.2f", g_sensor_data.accel_z / 16384.0);
            ssd1306_clear_area(5, 0, 128);
            ssd1306_puts(5, 78, line);



            /* ===== 刷到屏幕 (拿 I2C 总线锁) =====
             * 和 SensorTask 竞争 hi2c1, 拿不到锁就跳过这帧,
             * 等下一个 500ms 周期再试。 */
            if (osMutexAcquire(i2c1MutexHandle, 0) == osOK) {
                ssd1306_flush(&hi2c1);
                osMutexRelease(i2c1MutexHandle);
            }

            xSemaphoreGive(sensorDataMutexHandle);
        }

        /* 500ms 刷新一次 —— 人眼 2Hz 刷新足够看 */
        osDelay(500);
    }
}
