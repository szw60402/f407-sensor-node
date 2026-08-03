/**
 * @file  task_modbus.c
 * @brief ModbusTask —— 整个项目里最关键的任务
 *
 * 工作流程:
 *   1. 启动 USART3 DMA 循环接收 (DMA 在后台不停地往环形缓冲区写数据)
 *   2. 当总线上出现 ≥3.5 字符时间的静默 → USART3 IDLE 中断触发
 *      → ISR 回调中: ringbuf_commit_rx() + xSemaphoreGiveFromISR()
 *   3. ModbusTask 被信号量唤醒 → 从环形缓冲区读出所有字节
 *   4. 找 Modbus 帧 (地址0x01 + CRC校验通过)
 *   5. 处理功能码 (03=读寄存器, 06=写单寄存器)
 *   6. RS485 切到发送模式 → 回包 → 切回接收模式 → 重启 DMA
 *
 * 为什么用 IDLE 中断而不是 DMA 完成中断?
 *   Modbus 帧长度不固定 (4~256 字节), DMA 不知道要收多少字节。
 *   但 Modbus 协议规定帧间间隔 ≥3.5 字符时间, 所以用"线路空闲"
 *   来检测帧结束比用 DMA 完成中断更实用。
 */

#include "task_modbus.h"
#include "app_global.h"
#include "modbus_protocol.h"
#include "drivers/max485.h"
#include "utils/ringbuf.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "usart.h"
#include "dma.h"

/* ---------- 外部变量 (在 freertos.c 中定义) ---------- */
extern ringbuf_t g_modbus_rb;                /* 接收环形缓冲区 */
extern osSemaphoreId_t modbusRxSemaphoreHandle; /* IDLE中断→任务 通知信号量 */
extern DMA_HandleTypeDef hdma_usart3_rx;     /* USART3 RX DMA 句柄 */

/* Modbus 单帧最大 256 字节 (ADU 定义) */
#define MB_BUF_SIZE  256

void ModbusTaskFunc(void *argument)
{
    (void)argument;

    uint8_t mb_rx[MB_BUF_SIZE];  /* 接收缓冲区 */
    uint8_t mb_tx[MB_BUF_SIZE];  /* 发送缓冲区 (应答帧) */

    /* ---------- 启动 USART3 DMA 循环接收 ----------
     * HAL_UARTEx_ReceiveToIdle_DMA:
     *   → DMA 从 USART3 收数据, 循环写入 g_modbus_rb.buffer
     *   → 当总线空闲 (IDLE 帧检测) 时触发 RxEventCallback
     * 注意: 必须禁用半传输中断 (DMA_IT_HT), 否则每收一半字节就
     *        进一次中断, 把一帧数据切成两半。 */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, (uint8_t *)g_modbus_rb.buffer, RINGBUF_SIZE);
    __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);

    for (;;) {

        /* ====== 阻塞等待 IDLE 信号量 ======
         * 这个信号量在 HAL_UARTEx_RxEventCallback (ISR 回调) 里被 Give。
         * portMAX_DELAY = 永远等, 没帧到就休眠, 不浪费 CPU。 */
        if (xSemaphoreTake(modbusRxSemaphoreHandle, portMAX_DELAY) == pdTRUE) {

            /* 检查缓冲区有没有收到数据 */
            uint16_t rx_cnt = ringbuf_available(&g_modbus_rb);
            if (rx_cnt == 0) continue;

            /* 从环形缓冲区全部读出 */
            uint16_t len = ringbuf_read(&g_modbus_rb, mb_rx, MB_BUF_SIZE);

            /* ----- 核心: 解析 Modbus 帧 + 组装应答 -----
             * modbus_process() 做三件事:
             *   1. 验证从站地址 (必须是 0x01)
             *   2. 验证 CRC-16 (对不上 → 不回复)
             *   3. 解析 03/06 功能码 → 生成应答帧
             * 返回值 0 = 不需要回复 (CRC错/地址不匹配/广播) */
            uint16_t resp_len = modbus_process(mb_rx, len, mb_tx, MB_BUF_SIZE);
            if (resp_len == 0) goto restart_dma;  /* 跳过回复, 重开 DMA */

            /* ----- RS485 发送应答 ----- */
            max485_tx_mode();               /* 切换到发送模式 */
            osDelay(1);                     /* 等 RE/DE 电平稳定 (1ms) */

            HAL_UART_Transmit(&huart3, mb_tx, resp_len, 50);  /* 阻塞发送 */

            /* 等待发送完成标志 (Transmit Complete)。
             * 必须等 TC, 不能只等 TXE —— TXE=发送缓冲空, 数据还在移位寄存器里! */
            while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET) {
                /* 忙等 TC 标志 —— 嵌入式里少量场景允许, 不阻塞调度 */
            }
            __HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_TC);

            max485_rx_mode();               /* 切回接收模式 */

        restart_dma:
            /* 重启 DMA 循环接收 (因为上一轮被 IDLE 中断停掉了) */
            HAL_UARTEx_ReceiveToIdle_DMA(&huart3,
                (uint8_t *)g_modbus_rb.buffer, RINGBUF_SIZE);
            __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
        }
    }
}
