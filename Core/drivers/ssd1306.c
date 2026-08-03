/**
 * @file  ssd1306.c
 * @brief SSD1306 0.96寸 128×64 OLED 屏幕驱动
 *
 * 工作原理 (新手友好版):
 *   SSD1306 内部有一块 128×64 的显存 (GDDRAM), 每个 bit 对应屏幕上一个
 *   像素点。屏幕被分成 8 个"页"(Page), 每页 8 行 × 128 列。
 *
 *   往某页的某列写 1 个字节 → 那一列上 8 个像素一次亮灭。
 *   要刷新全屏: 把 8 页全部重写一遍。
 *
 * 本驱动的做法:
 *   在 MCU 内存里建一个 framebuffer[8][128] (1024 字节),
 *   所有画图操作都先改 framebuffer, 最后调 flush() 一次性
 *   把 8 页全部写到屏幕的 GDDRAM 里。
 *
 *   → 好处: 避免频繁的 I²C 通信, 屏幕不闪。
 *   → 代价: 多占 1KB RAM (F407 有 192KB, 完全不慌)。
 */

#include "ssd1306.h"
#include <string.h>

/* ====== 5×7 ASCII 字体表 (ASCII 0x20-0x7E) ======
 * 每个字符 5 列, 每列 1 字节 (代表该列上 8 个像素的亮灭)。
 * 最常用的嵌入式字体方案, 几乎每个 OLED 项目都用这套字模。 */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* sp (空格) */
    {0x00,0x00,0x5F,0x00,0x00}, /* !  */
    {0x00,0x07,0x00,0x07,0x00}, /* "  */
    {0x14,0x7F,0x14,0x7F,0x14}, /* #  */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $  */
    {0x23,0x13,0x08,0x64,0x62}, /* %  */
    {0x36,0x49,0x55,0x22,0x50}, /* &  */
    {0x00,0x05,0x03,0x00,0x00}, /* '  */
    {0x00,0x1C,0x22,0x41,0x00}, /* (  */
    {0x00,0x41,0x22,0x1C,0x00}, /* )  */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* *  */
    {0x08,0x08,0x3E,0x08,0x08}, /* +  */
    {0x00,0x50,0x30,0x00,0x00}, /* ,  */
    {0x08,0x08,0x08,0x08,0x08}, /* -  */
    {0x00,0x60,0x60,0x00,0x00}, /* .  */
    {0x20,0x10,0x08,0x04,0x02}, /* /  */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0  */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1  */
    {0x42,0x61,0x51,0x49,0x46}, /* 2  */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3  */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4  */
    {0x27,0x45,0x45,0x45,0x39}, /* 5  */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6  */
    {0x01,0x71,0x09,0x05,0x03}, /* 7  */
    {0x36,0x49,0x49,0x49,0x36}, /* 8  */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9  */
    {0x00,0x36,0x36,0x00,0x00}, /* :  */
    {0x00,0x56,0x36,0x00,0x00}, /* ;  */
    {0x00,0x08,0x14,0x22,0x41}, /* <  */
    {0x14,0x14,0x14,0x14,0x14}, /* =  */
    {0x41,0x22,0x14,0x08,0x00}, /* >  */
    {0x02,0x01,0x51,0x09,0x06}, /* ?  */
    {0x32,0x49,0x79,0x41,0x3E}, /* @  */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A  */
    {0x7F,0x49,0x49,0x49,0x36}, /* B  */
    {0x3E,0x41,0x41,0x41,0x22}, /* C  */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D  */
    {0x7F,0x49,0x49,0x49,0x41}, /* E  */
    {0x7F,0x09,0x09,0x01,0x01}, /* F  */
    {0x3E,0x41,0x41,0x51,0x32}, /* G  */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H  */
    {0x00,0x41,0x7F,0x41,0x00}, /* I  */
    {0x20,0x40,0x41,0x3F,0x01}, /* J  */
    {0x7F,0x08,0x14,0x22,0x41}, /* K  */
    {0x7F,0x40,0x40,0x40,0x40}, /* L  */
    {0x7F,0x02,0x04,0x02,0x7F}, /* M  */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N  */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O  */
    {0x7F,0x09,0x09,0x09,0x06}, /* P  */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q  */
    {0x7F,0x09,0x19,0x29,0x46}, /* R  */
    {0x46,0x49,0x49,0x49,0x31}, /* S  */
    {0x01,0x01,0x7F,0x01,0x01}, /* T  */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U  */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V  */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W  */
    {0x63,0x14,0x08,0x14,0x63}, /* X  */
    {0x03,0x04,0x78,0x04,0x03}, /* Y  */
    {0x61,0x51,0x49,0x45,0x43}, /* Z  */
    {0x00,0x00,0x7F,0x41,0x41}, /* [  */
    {0x02,0x04,0x08,0x10,0x20}, /* \  */
    {0x41,0x41,0x7F,0x00,0x00}, /* ]  */
    {0x04,0x02,0x01,0x02,0x04}, /* ^  */
    {0x40,0x40,0x40,0x40,0x40}, /* _  */
    {0x00,0x01,0x02,0x04,0x00}, /* `  */
};

/* ====== MCU 端的屏幕缓冲区 ======
 * fb[页号][列号] = 该列 8 个像素的亮灭状态。
 * 所有画图操作改这个数组, flush() 时整帧写到屏幕。 */
static uint8_t fb[SSD1306_PAGES][SSD1306_WIDTH];

/* 保存 I²C 句柄, 内部 send_cmd/send_data 用 */
static I2C_HandleTypeDef *ssd1306_i2c;

/* ==================== 内部函数 ==================== */

/**
 * @brief 向 SSD1306 发送一条命令 (如"选页"、"开显示")
 *
 * I²C 格式: [0x00][命令字节]
 *   0x00 是"控制字节" —— 告诉 SSD1306 "接下来是命令, 不是像素数据"
 */
static void send_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(ssd1306_i2c, SSD1306_I2C_ADDR << 1, buf, 2, 100);
}

/**
 * @brief 向 SSD1306 发送像素数据 (一页上的若干列)
 *
 * 因为 HAL I²C 单次最多发 ~255 字节 (分页模式下), 超过就拆成多包。
 * 控制字节 = 0x40, 告诉 SSD1306 "接下来是像素数据"
 */
static void send_data(uint8_t *data, uint16_t len)
{
    uint8_t buf[129];  /* 1 字节控制 + 最多 128 字节数据 */
    while (len > 0) {
        uint16_t chunk = (len > 128) ? 128 : len;
        buf[0] = 0x40;                     /* 控制字节: 数据 */
        memcpy(buf + 1, data, chunk);      /* 复制像素 */
        HAL_I2C_Master_Transmit(ssd1306_i2c, SSD1306_I2C_ADDR << 1, buf, chunk + 1, 200);
        data += chunk;
        len  -= chunk;
    }
}

/* ==================== 公开函数 ==================== */

/**
 * @brief 初始化 SSD1306
 *
 * 这个初始化序列有 20 多条命令, 全部来自 SSD1306 Datasheet
 * 第 9 页的"初始化流程图"。参数的意义不需要背, 只要知道:
 *   → 设置寻址模式、扫描方向、对比度、时钟频率等
 *   → 最后一条 0xAF = 打开显示 (之前都是隐形配置)
 *
 * 如果你的 OLED 显不了字:
 *   1. 先跑 I²C 扫描确认地址 0x3C 在线
 *   2. 检查 VCC/GND 是否接反
 *   3. 用逻辑分析仪看 SCL/SDA 有没有波形
 */
void ssd1306_init(I2C_HandleTypeDef *hi2c)
{
    ssd1306_i2c = hi2c;
    HAL_Delay(10);  /* 等屏幕电源稳定 */

    /* 下面这一段不需要背, 直接 CV 就行。
     * 这是所有 SSD1306 OLED 的标准初始化序列。 */
    send_cmd(0xAE); /* 关闭显示 (配完了再开) */
    send_cmd(0x20); send_cmd(0x00); /* 水平寻址模式 */
    send_cmd(0xB0); /* 从 Page 0 开始 */
    send_cmd(0xC8); /* COM 从上到下扫描 */
    send_cmd(0x00); send_cmd(0x10); /* 列起始地址 */
    send_cmd(0x40); /* 显示起始行 = 0 */
    send_cmd(0x81); send_cmd(0x7F); /* 对比度 = 127 (0-255) */
    send_cmd(0xA1); /* 段重映射 (左右不翻转) */
    send_cmd(0xA6); /* 正常显示 (A7=反色) */
    send_cmd(0xA8); send_cmd(0x3F); /* 多路复用比 = 63 (64 行-1) */
    send_cmd(0xA4); /* 全屏点亮关闭 */
    send_cmd(0xD3); send_cmd(0x00); /* 显示偏移 = 0 (非 0 会把页内字符水平切半错位, 已回滚) */
    send_cmd(0xD5); send_cmd(0x80); /* 显示时钟分频 */
    send_cmd(0xD9); send_cmd(0xF1); /* 预充电周期 */
    send_cmd(0xDA); send_cmd(0x12); /* COM 引脚配置 */
    send_cmd(0xDB); send_cmd(0x40); /* VCOMH 电压 */
    send_cmd(0x8D); send_cmd(0x14); /* 开启电荷泵 (给 OLED 内部升压) */
    send_cmd(0xAF); /* ====== 打开显示! ====== */

    ssd1306_clear();
}

/** @brief 清空 framebuffer (让整个屏幕变黑) */
void ssd1306_clear(void)
{
    memset(fb, 0, sizeof(fb));
}

/** @brief 清空 framebuffer 的局部区域 */
void ssd1306_clear_area(uint8_t page, uint8_t col, uint8_t width)
{
    if (page < SSD1306_PAGES && (col + width) <= SSD1306_WIDTH)
        memset(&fb[page][col], 0, width);
}

/**
 * @brief 把 framebuffer 整帧写到屏幕
 *
 * 流程: 8 个 Page 逐个发:
 *   1. 发送命令 0xB0+N (选 Page N)
 *   2. 发送命令 0x00, 0x10 (列地址归零)
 *   3. 发送该 Page 的 128 字节像素数据
 */
void ssd1306_flush(I2C_HandleTypeDef *hi2c)
{
    (void)hi2c;  /* 使用全局 ssd1306_i2c, 参数保留供兼容 */
    for (uint8_t p = 0; p < SSD1306_PAGES; p++) {
        send_cmd(0xB0 + p);     /* 选页 */
        send_cmd(0x00);         /* 列地址低4位 = 0 */
        send_cmd(0x10);         /* 列地址高4位 = 0 */
        send_data(fb[p], SSD1306_WIDTH);  /* 这一页的 128 列全发 */
    }
}

/**
 * @brief 在指定位置画一个字符
 *
 * 字符占 5 像素宽 + 1 像素间距 = 共 6 列
 * 只支持 ASCII 0x20(空格) ~ 0x7E(~)
 */
void ssd1306_putc(uint8_t page, uint8_t col, char c)
{
    if (c < 0x20 || c > 0x60) c = ' ';  /* 字模表只到 0x60(反引号), 超出会越界读 FLASH 出乱码 */
    uint8_t idx = (uint8_t)c - 0x20;      /* 映射到字体表下标 */

    /* 把字模的 5 列抄到 framebuffer 里面 */
    for (uint8_t i = 0; i < 5 && (col + i) < SSD1306_WIDTH; i++)
        fb[page][col + i] = font5x7[idx][i];

    /* 字符间加 1 列空白, 否则字会连在一起 */
    if (col + 5 < SSD1306_WIDTH)
        fb[page][col + 5] = 0x00;
}

/**
 * @brief 画一整个字符串 (自动换列, 不换行)
 *
 * 每字符占 6 列 (5 像素 + 1 间距)。
 * 128÷6 ≈ 21 字符/行, 超出会截断。
 */
void ssd1306_puts(uint8_t page, uint8_t col, const char *str)
{
    while (*str) {
        if (col + 5 >= SSD1306_WIDTH) break;  /* 到行末不画了 */
        ssd1306_putc(page, col, *str++);
        col += 6;
    }
}
