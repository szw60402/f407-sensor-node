/**
 * @file  ssd1306.h
 * @brief SSD1306 0.96寸 OLED 驱动 (I²C, 128×64)
 *
 * I²C 地址: 0x3C
 * 用法:
 *   ssd1306_init(&hi2c1);
 *   ssd1306_clear();
 *   ssd1306_puts(0, 0, "Hello");  // (行, 列, 字符串)
 *   ssd1306_flush();               // 刷新到屏幕
 */

#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SSD1306_I2C_ADDR   0x3C
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT       64
#define SSD1306_PAGES         8   /* 64÷8=8 页 */

void ssd1306_init(I2C_HandleTypeDef *hi2c);
void ssd1306_clear(void);
void ssd1306_clear_area(uint8_t page, uint8_t col, uint8_t width);
void ssd1306_flush(I2C_HandleTypeDef *hi2c);
void ssd1306_putc(uint8_t page, uint8_t col, char c);
void ssd1306_puts(uint8_t page, uint8_t col, const char *str);

#endif
