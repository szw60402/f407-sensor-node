/**
 * @file  acs712.c
 * @brief ACS712-05B 电流传感器驱动
 *
 * 工作原理:
 *   ACS712 是霍尔效应电流传感器。它输出一个模拟电压,
 *   电压 = 2.5V(零点) + 电流 × 0.185V/A(灵敏度)。
 *
 *   正向电流 → 电压高于 2.5V, 反向电流 → 电压低于 2.5V。
 *
 * 电路 (面包板上):
 *   ACS712 OUT → 13kΩ → PA0(STM32 ADC) → 27kΩ → GND
 *
 *   这个分压网络把 0~5V 的信号缩小到 0~3.3V 以内,
 *   因为 STM32 ADC 只能测 0~3.3V。分压比 = 27/(13+27) = 0.675。
 *
 * ADC 到电流的换算:
 *   ADC值 → 电压(3.3V参考) → 反推分压前的 OUT 电压 → 减去零点 → 除以灵敏度
 */

#include "acs712.h"

/* ====== 校准后的零点电压 ======
 * 出厂用 5V 标准零点 2.50V, 实际模块供电 4.87V → 零点 2.435V,
 * 不校准悬空会显示 ±0.5A 假偏移。上电空载校准后这里存真实零点。 */
static double g_zero_v = ACS712_ZERO_V;

/* ====== 零点校准: 上电空载时调用, 用平均 ADC 值算出真实零点 ======
 * 参数 adc_val 是多次采样的平均值 (调用方采样 32 次取平均)。
 * 校准前提: 上电瞬间负载未接 (ACS712 空载输出 = VCC/2)。 */
void acs712_calibrate_zero(ADC_HandleTypeDef *hadc, uint32_t adc_val)
{
    (void)hadc;
    double v_adc = (3.30 * adc_val) / 4095.0;
    g_zero_v = v_adc / ACS712_DIV_RATIO;
}

double acs712_read_current(ADC_HandleTypeDef *hadc, uint32_t adc_val)
{
    (void)hadc;  /* ADC 句柄保留供以后多通道扩展 */
    /* ---------- 第1步: ADC 原始值 → 电压 ----------
     * STM32 ADC 是 12-bit, 参考电压 3.30V。
     * 4095 对应 3.30V, 0 对应 0V —— 线性关系 */
    double v_adc = (3.30 * adc_val) / 4095.0;

    /* ---------- 第2步: 反推 ACS712 OUT 引脚的真实电压 ----------
     * ADC 读到的电压是被 13k+27k 分压后的结果。
     * 要得到分压前的电压: V_out = V_adc ÷ 0.675 */
    double v_out = v_adc / ACS712_DIV_RATIO;

    /* ---------- 第3步: 电压 → 电流 ----------
     * 零点 = 上电校准值 (默认 2.50V), 灵敏度 = 0.185 V/A
     * 电流 = (当前电压 - 零点电压) ÷ 灵敏度 */
    double current = (v_out - g_zero_v) / ACS712_SENS_V_A;

    return current;  /* >0 = 正向电流(流入 IP+), <0 = 反向电流 */
}
