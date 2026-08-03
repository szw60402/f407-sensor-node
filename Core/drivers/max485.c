#include "max485.h"

void max485_init(void)
{
    /* GPIO 已在 CubeMX 中初始化, 默认输出低 = 接收模式 */
    max485_rx_mode();
}

void max485_tx_mode(void)
{
    HAL_GPIO_WritePin(MAX485_DE_PORT, MAX485_DE_PIN, GPIO_PIN_SET);
}

void max485_rx_mode(void)
{
    HAL_GPIO_WritePin(MAX485_DE_PORT, MAX485_DE_PIN, GPIO_PIN_RESET);
}
