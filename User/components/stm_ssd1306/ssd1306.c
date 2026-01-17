#include <stdint.h>
#include "ssd1306.h"
#include "ssd1306_internal.h"

void ssd1306_init(void)
{
    // 1. 硬件复位
    ssd1306_reset();

    // 2. 关闭屏幕（初始化期间）
    ssd1306_tx_cmd(0xAE, NULL, 0); // SSD1306_CMD_DISP_OFF

    // 3. 基础配置 (参考 ESP-IDF 及标准手册)
    // 设置寻址模式为“页寻址模式”，以匹配 LVGL 缓冲区的转换逻辑
    ssd1306_tx_cmd(0x20, (uint8_t[]){0x00}, 1); // SSD1306_CMD_SET_MEMORY_ADDR_MODE -> Page Addressing Mode

    // 设置屏幕尺寸和 COM 引脚配置
    ssd1306_tx_cmd(0xA8, (uint8_t[]){SSD1306_HEIGHT - 1}, 1); // SSD1306_CMD_SET_MULTIPLEX
    ssd1306_tx_cmd(0xDA, (uint8_t[]){0x12}, 1); // SSD1306_CMD_SET_COMPINS (适用于 64 高度)

    // 4. 显示方向/镜像设置 (通常需要开启以获得正确的显示方向)
    ssd1306_tx_cmd(0xA1, NULL, 0); // SSD1306_CMD_SEG_REMAP_ON, 列地址 127 映射到 0
    ssd1306_tx_cmd(0xC8, NULL, 0); // SSD1306_CMD_COM_SCAN_DIR_REMAPPED, 从 COM[N-1] 到 0

    // 5. 时序和驱动电压设置 (关键配置)
    ssd1306_tx_cmd(0xD5, (uint8_t[]){0x80}, 1); // SSD1306_CMD_SET_DISP_CLOCK_DIV, 设置时钟分频/振荡器频率
    ssd1306_tx_cmd(0xD9, (uint8_t[]){0xF1}, 1); // SSD1306_CMD_SET_PRECHARGE, 设置预充电周期
    ssd1306_tx_cmd(0xDB, (uint8_t[]){0x30}, 1); // SSD1306_CMD_SET_VCOM_DESELECT, 设置 VCOMH

    // 6. 启用电荷泵 (内部升压电路)
    ssd1306_tx_cmd(0x8D, (uint8_t[]){0x14}, 1); // SSD1306_CMD_SET_CHARGE_PUMP

    // 7. 设置显示模式
    ssd1306_tx_cmd(0xA4, NULL, 0); // SSD1306_CMD_ENTIRE_DISP_OFF, 恢复 GDDRAM 显示
    ssd1306_tx_cmd(0xA7, NULL, 0); // SSD1306_CMD_SET_NORMAL_DISP, 设置为反色显示(0为点亮)

    // 8. 清屏 (可选, 但推荐)
    // uint8_t clear_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8] = {0};
    // ssd1306_draw(clear_buffer); // 您需要一个发送全屏数据的函数

    // 9. 打开屏幕
    ssd1306_tx_cmd(0xAF, NULL, 0); // SSD1306_CMD_DISP_ON
}

void ssd1306_draw(uint8_t *color_data)
{
    // 1. 设置列地址范围 (从第 0 列到第 127 列)
    ssd1306_tx_cmd(0x21, (uint8_t[]){0, SSD1306_WIDTH - 1}, 2);

    // 2. 设置页地址范围 (从第 0 页到第 7 页)
    ssd1306_tx_cmd(0x22, (uint8_t[]){0, (SSD1306_HEIGHT / 8) - 1}, 2);

    // 3. 通过 SPI 发送整个缓冲区的数据
    // 在此操作之前，DC 引脚必须被设置为 DATA 模式
    ssd1306_tx_data(color_data, FULL_FLUSH_BUFFER_SIZE);
}
