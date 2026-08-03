/**
 * @file  ringbuf.h
 * @brief 循环缓冲区（用于 USART3 串口 DMA 接收）
 *
 * 用法:
 *   在 USART3 IDLE 中断中: ringbuf_commit_rx(&rb, dma_rcvd_count);
 *   在 ModbusTask 中:       len = ringbuf_read(&rb, frame_buf, sizeof(frame_buf));
 *                          查找 Modbus 帧 → 解析→ 回包
 */

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stddef.h>

#define RINGBUF_SIZE 512

typedef struct {
    volatile uint8_t buffer[RINGBUF_SIZE];
    volatile uint16_t head;   /* 写入位置(DMA 控制) */
    volatile uint16_t tail;   /* 读取位置(任务控制) */
    volatile uint16_t count;  /* 缓冲区已有字节数 */
} ringbuf_t;

/**
 * @brief 初始化环形缓冲区
 */
void ringbuf_init(ringbuf_t *rb);

/**
 * @brief DMA 接收完成 -> 已收到 len 字节写入缓冲区
 * @note  在 HAL_UARTEx_RxEventCallback 中调用
 */
void ringbuf_commit_rx(ringbuf_t *rb, uint16_t len);

/**
 * @brief 从缓冲区读出最多 max_len 字节
 * @return 实际读出的字节数
 */
uint16_t ringbuf_read(ringbuf_t *rb, uint8_t *out, uint16_t max_len);

/**
 * @brief 返回缓冲区当前可用字节数
 */
uint16_t ringbuf_available(const ringbuf_t *rb);

/**
 * @brief 清空缓冲区
 */
void ringbuf_clear(ringbuf_t *rb);

#endif
