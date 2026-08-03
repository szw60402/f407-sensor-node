#include "tja1050.h"

HAL_StatusTypeDef can_send_frame(CAN_HandleTypeDef *hcan,
                                  uint32_t id, const uint8_t *data, uint8_t len)
{
    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;

    if (len > 8) len = 8;

    tx_header.StdId   = id & 0x7FF;
    tx_header.ExtId   = 0;
    tx_header.IDE     = CAN_ID_STD;
    tx_header.RTR     = CAN_RTR_DATA;
    tx_header.DLC     = len;
    tx_header.TransmitGlobalTime = DISABLE;

    return HAL_CAN_AddTxMessage(hcan, &tx_header, (uint8_t *)data, &tx_mailbox);
}

HAL_StatusTypeDef can_send_sensor_frame(CAN_HandleTypeDef *hcan,
                                         int16_t temp_x10, uint16_t hum_x10,
                                         int16_t current_ma, uint8_t light_pct)
{
    uint8_t data[8];
    data[0] = (temp_x10 >> 8) & 0xFF;
    data[1] = temp_x10 & 0xFF;
    data[2] = (hum_x10 >> 8) & 0xFF;
    data[3] = hum_x10 & 0xFF;
    data[4] = (current_ma >> 8) & 0xFF;
    data[5] = current_ma & 0xFF;
    data[6] = light_pct;
    data[7] = 0x00;  /* reserved */

    return can_send_frame(hcan, 0x100, data, 8);
}
