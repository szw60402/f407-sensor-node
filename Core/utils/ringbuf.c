/**
 * @file  ringbuf.c
 * @brief 循环缓冲区 —— 解决串口 DMA 收数据和任务读数据的"生产-消费"问题
 *
 * 为什么需要这个东西?
 *   USART3 用 DMA 方式收 Modbus 数据 → DMA 直接把字节写到内存里,
 *   不经过 CPU。ModbusTask 不知道什么时候有了新数据。
 *
 *   这个环形缓冲区是 DMA (生产者) 和 ModbusTask (消费者) 之间的桥梁:
 *     - DMA 收到数据 → IDLE 中断 → commit_rx(buf, 收到的字节数)
 *     - ModbusTask 被信号量唤醒 → read() 读出全部字节 → 找 Modbus 帧
 *
 * 数据结构:
 *   buffer[512]  ← DMA 往里写 (head 往后走)
 *                ← 任务往外读 (tail 往后走)
 *   当 head 或 tail 走到末尾 → 绕回开头 (所以叫"环形")
 *   count = 缓冲区里现有多少字节还没被读走
 */

#include "ringbuf.h"
#include <string.h>

void ringbuf_init(ringbuf_t *rb)
{
    memset((void *)rb->buffer, 0, RINGBUF_SIZE);
    rb->head  = 0;   /* 写指针: 下个字节该存到哪个位置 */
    rb->tail  = 0;   /* 读指针: 下个字节该从哪读 */
    rb->count = 0;   /* 当前缓冲了多少个字节 */
}

/**
 * @brief DMA 收到 len 字节 → 通知缓冲区
 *
 * 调用时机: HAL_UARTEx_RxEventCallback 中 (USART3 IDLE 中断触发时)
 *
 * 溢出保护: 如果数据太多塞不下, 整包丢弃 (防止脏数据污染 buffer)
 */
void ringbuf_commit_rx(ringbuf_t *rb, uint16_t len)
{
    if (rb->count + len > RINGBUF_SIZE) {
        /* 溢出! 清空缓冲区, 这帧数据放弃。
         * 不清空的话 head/tail 会错位, 后续所有 Modbus 帧全乱。 */
        rb->head  = 0;
        rb->tail  = 0;
        rb->count = 0;
        return;
    }
    /* head 前移 len 步, 绕回机制用取模运算实现。
     * 例: head=510, len=5 → (510+5) % 512 = 3 → 绕到开头 */
    rb->head  = (rb->head + len) % RINGBUF_SIZE;
    rb->count += len;
}

/**
 * @brief 从缓冲区取出最多 max_len 字节
 * @return 实际取出的字节数 (可能比 max_len 少, 说明缓冲区不够)
 */
uint16_t ringbuf_read(ringbuf_t *rb, uint8_t *out, uint16_t max_len)
{
    uint16_t avail = rb->count;
    if (avail == 0) return 0;

    /* 不要超过实际可用字节数 */
    if (max_len > avail) max_len = avail;

    /* 逐字节拷到 out 数组里 */
    for (uint16_t i = 0; i < max_len; i++) {
        out[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % RINGBUF_SIZE;  /* tail 前移, 可能绕回 */
    }
    rb->count -= max_len;
    return max_len;
}

uint16_t ringbuf_available(const ringbuf_t *rb)
{
    return rb->count;
}

void ringbuf_clear(ringbuf_t *rb)
{
    rb->head  = 0;
    rb->tail  = 0;
    rb->count = 0;
}
