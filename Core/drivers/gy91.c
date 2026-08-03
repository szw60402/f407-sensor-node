/**
 * @file  gy91.c
 * @brief GY-91 模块姿态驱动 (实测芯片 = MPU-6500, 假 GY-91)
 *
 * ⚠️ 2026-08-03 实测: 模块标称 GY-91 (ICM-20948), 但 0x75 寄存器
 * 读到 0x70 = MPU-6500 —— 假模块用 MPU-6500 冒充 ICM-20948。
 * 本驱动已按 MPU-6500 适配:
 *   - WHO_AM_I @ 0x75 (=0x70), 数据从 0x3B 起连续读 14 字节
 *   - 无 Bank 机制 (ICM-20948 才有), 初始化只需唤醒 + 量程
 * 若以后换回真 ICM-20948 模块, 需改回 Bank 版驱动。
 */

#include "gy91.h"

volatile uint32_t dbg_whoami = 0;   /* 诊断探针: WHO_AM_I 芯片 ID */

/* ---------- 寄存器地址 (MPU-6500 手册) ----------
 * ⚠️ 2026-08-03 实测: 本模块 0x75 寄存器 = 0x70, 是 MPU-6500,
 * 不是标称的 ICM-20948 (0xEA)。假 GY-91 模块常见做法。
 * 驱动已按 MPU-6500 适配 (无 Bank 机制, 比 ICM-20948 简单)。 */
#define REG_WHO_AM_I      0x75   /* 芯片 ID (MPU-6500 = 0x70) */
#define REG_PWR_MGMT_1    0x6B   /* 电源管理: bit6=SLEEP, 写 0x00 唤醒 */
#define REG_ACCEL_CONFIG  0x1C   /* 加速度量程 (±2g = 0x00) */
#define REG_GYRO_CONFIG   0x1B   /* 陀螺仪量程 (±250dps = 0x00) */
#define REG_ACCEL_XOUT_H  0x3B   /* 加速度 X 高字节 (数据起始地址) */

#define GY91_WHO_AM_I_VAL  0x70   /* MPU-6500 出厂 ID */

#define BUF_SIZE 14  /* MPU-6500: 加速度6 + 温度2 + 陀螺仪6 */

/* ====== 内部辅助: 往芯片写一个寄存器值 ====== */
static HAL_StatusTypeDef reg_write(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val)
{
    /* I²C 写寄存器格式: [寄存器地址][数据值]
     * buf[0]=寄存器号, buf[1]=要写的值, 共 2 字节 */
    uint8_t buf[2] = {reg, val};
    return HAL_I2C_Master_Transmit(hi2c, GY91_I2C_ADDR << 1, buf, 2, 100);
}

/**
 * @brief 初始化 MPU-6500 (本模块实测芯片, 假 GY-91)
 *
 * 步骤 (按手册顺序):
 *   1. 验证芯片存在 (WHO_AM_I @ 0x75, 应为 0x70)
 *   2. 写 PWR_MGMT_1 (0x6B) = 0x00: 唤醒
 *   3. 等 15ms 让内部振荡器稳定
 *   4. 配 ACCEL_CONFIG: ±2g, 灵敏度 16384 LSB/g
 *   5. 配 GYRO_CONFIG: ±250dps
 *   (MPU-6500 无 Bank 机制, 不需要切页)
 */
HAL_StatusTypeDef gy91_init(I2C_HandleTypeDef *hi2c)
{
    uint8_t whoami;
    HAL_StatusTypeDef ret;

    /* 1. 读 WHO_AM_I —— 先发寄存器地址 0x75, 再收 1 字节 */
    uint8_t reg = REG_WHO_AM_I;
    ret = HAL_I2C_Master_Transmit(hi2c, GY91_I2C_ADDR << 1, &reg, 1, 100);
    if (ret != HAL_OK) return ret;
    ret = HAL_I2C_Master_Receive(hi2c, GY91_I2C_ADDR << 1, &whoami, 1, 100);
    if (ret != HAL_OK) return ret;
    dbg_whoami = whoami;   /* 诊断探针: 芯片 ID, MPU-6500 应为 0x70 */

    /* 2. 唤醒: 清 PWR_MGMT_1 的 SLEEP 位 */
    reg_write(hi2c, REG_PWR_MGMT_1, 0x00);
    HAL_Delay(15);  /* 等内部振荡器起振 */

    /* 3. 配加速度量程: ±2g */
    reg_write(hi2c, REG_ACCEL_CONFIG, GY91_ACCEL_2G << 1);

    /* 4. 配陀螺仪量程: ±250dps */
    reg_write(hi2c, REG_GYRO_CONFIG, GY91_GYRO_250 << 1);

    /* 诊断探针: 读回关键寄存器验证写入生效
     * 预期: PWR_MGMT_1(0x6B)=0x00 (SLEEP 清), ACCEL_CONFIG(0x1C)=0x00 (±2g) */
    uint8_t r_pwr = 0x6B, r_cfg = 0x1C, v_pwr = 0xFF, v_cfg = 0xFF;
    HAL_I2C_Master_Transmit(hi2c, GY91_I2C_ADDR << 1, &r_pwr, 1, 100);
    HAL_I2C_Master_Receive(hi2c, GY91_I2C_ADDR << 1, &v_pwr, 1, 100);
    HAL_I2C_Master_Transmit(hi2c, GY91_I2C_ADDR << 1, &r_cfg, 1, 100);
    HAL_I2C_Master_Receive(hi2c, GY91_I2C_ADDR << 1, &v_cfg, 1, 100);
    dbg_whoami |= ((uint32_t)v_pwr << 8) | ((uint32_t)v_cfg << 16);
    /* 低8位=WHO_AM_I, 8-15位=PWR_MGMT_1, 16-23位=ACCEL_CONFIG */

    return HAL_OK;
}

/**
 * @brief 一次读取加速度 + 陀螺仪
 *
 * MPU-6500 寄存器布局 (连续读):
 *   0x3B 起: ACCEL_X(2) ACCEL_Y(2) ACCEL_Z(2) TEMP(2) GYRO_X(2) GYRO_Y(2) GYRO_Z(2)
 *   → 从 0x3B 连读 14 字节, 一次拿全
 *
 * 字节排列 (大端序, MSB 在前):
 *   [0:1] = ACCEL_X  [2:3] = ACCEL_Y  [4:5] = ACCEL_Z
 *   [6:7] = TEMP     [8:9] = GYRO_X   [10:11]= GYRO_Y  [12:13]= GYRO_Z
 */
HAL_StatusTypeDef gy91_read(I2C_HandleTypeDef *hi2c,
                                 int16_t *accel_x, int16_t *accel_y, int16_t *accel_z,
                                 int16_t *gyro_x,  int16_t *gyro_y,  int16_t *gyro_z)
{
    uint8_t buf[14];  /* 加速度6 + 温度2 + 陀螺仪6 */
    HAL_StatusTypeDef ret;
    uint8_t reg = REG_ACCEL_XOUT_H;  /* 起始地址 = 0x3B */

    /* 告诉芯片: "我要从 0x3B 开始读" */
    ret = HAL_I2C_Master_Transmit(hi2c, GY91_I2C_ADDR << 1, &reg, 1, 100);
    if (ret != HAL_OK) return ret;

    /* 连续收 14 字节 */
    ret = HAL_I2C_Master_Receive(hi2c, GY91_I2C_ADDR << 1, buf, 14, 100);
    if (ret != HAL_OK) return ret;

    /* 大端 → 小端 (ARM 是小端): 高字节拼低字节 */
    *accel_x = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    *accel_y = (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    *accel_z = (int16_t)(((uint16_t)buf[4] << 8) | buf[5]);
    *gyro_x  = (int16_t)(((uint16_t)buf[8] << 8) | buf[9]);
    *gyro_y  = (int16_t)(((uint16_t)buf[10] << 8) | buf[11]);
    *gyro_z  = (int16_t)(((uint16_t)buf[12] << 8) | buf[13]);

    return HAL_OK;
}
