/**
 * @file  max485.h
 * @brief MAX485CSA RS485 收发控制
 *
 * RE(接收使能)+DE(发送使能) 并联到 PC4
 * PC4=HIGH -> 发送模式, PC4=LOW -> 接收模式
 */

#ifndef MAX485_H
#define MAX485_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define MAX485_DE_PORT  GPIOC
#define MAX485_DE_PIN   GPIO_PIN_4

void max485_init(void);
void max485_tx_mode(void);   /* 发送模式 */
void max485_rx_mode(void);   /* 接收模式 */

#endif
