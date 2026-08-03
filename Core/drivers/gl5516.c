/**
 * @file  gl5516.c
 * @brief GL5516 光敏电阻驱动
 *
 * 电路 (面包板上):
 *   3.3V → GL5516 → PA1(ADC) → 10kΩ → GND
 *
 * 这是一个分压电路:
 *   光照强 → GL5516 电阻变小 (~5kΩ) → PA1 电压升高 (~2.2V) → ADC 读数大
 *   光照弱 → GL5516 电阻变大 (~500kΩ) → PA1 电压降低 (~0V) → ADC 读数小
 *
 * 注意: GL5516 是硫化镉光敏电阻, 阻值离散性非常大。
 * 标称值 5-10kΩ @ 10Lux, 但实际值因批次/温度而异。
 * 所以本驱动只用相对百分比 (0%=暗, 100%=亮), 不追求绝对照度。
 */

#include "gl5516.h"

double gl5516_read_voltage(uint32_t adc_val)
{
    /* ADC 12-bit, 参考电压 3.30V */
    return (3.30 * adc_val) / 4095.0;
}

/**
 * @brief 估算相对光照强度
 *
 * 这个函数不精确 —— 因为 GL5516 的阻值-光照曲线不是线性的。
 * 但作为"暗/亮"判断已经够了。
 *
 * 阈值来自实测:
 *   室内台灯直射 → ADC≈3000 (约 2.4V) → 判定 100%
 *   手完全遮住    → ADC≈100  (约 0.08V)→ 判定 0%
 */
uint8_t gl5516_light_pct(uint32_t adc_val)
{
    double voltage = gl5516_read_voltage(adc_val);

    if (voltage < 0.1) return 0;       /* 基本全黑 */
    if (voltage > 2.3) return 100;     /* 非常亮 */

    /* 中间状态: 线性映射 0.1V→0%, 2.3V→100% */
    return (uint8_t)((voltage - 0.1) / 2.2 * 100.0);
}
