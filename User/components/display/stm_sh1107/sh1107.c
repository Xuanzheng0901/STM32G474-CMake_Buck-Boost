#include<stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "display_internal.h"
#include "sh1107_internal.h"

void sh1107_reset(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	vTaskDelay(10);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
	vTaskDelay(10);
}

void sh1107_init(void)
{
	// 1. 硬件复位
	sh1107_reset();

	// 2. 关闭显示
	display_tx_cmd(SH1107_CMD_DISP_OFF, NULL, 0);

	// 3. 设置反向显示(旋转180度)
	// display_tx_cmd(SH1107_CMD_SET_SEGMENT_REMAP_REVERSE, NULL, 0);
	// display_tx_cmd(SH1107_CMD_SET_COM_SCAN_REVERSE, NULL, 0);

	display_tx_cmd(SH1107_CMD_SET_CLOCK_DIV, (uint8_t[]){0xF0}, 1);
	// 6. 驱动电压与对比度
	display_tx_cmd(SH1107_CMD_SET_DISPLAY_CONTRAST, (uint8_t[]){0x40}, 1);
	display_tx_cmd(SH1107_CMD_SET_PRECHARGE_PERIOD, (uint8_t[]){0x22}, 1);
	display_tx_cmd(SH1107_CMD_SET_VCOM_LEVEL, (uint8_t[]){0x37}, 1);
	// 7. 设置显示模式
	display_tx_cmd(SH1107_CMD_SET_DISPLAY_INVERT_ON, NULL, 0);

	// 8. 打开屏幕
	display_tx_cmd(SH1107_CMD_DISP_ON, NULL, 0);
}

void sh1107_draw(uint8_t *buffer)
{
	for(uint8_t page = 0; page < (SH1107_HEIGHT / 8); page++)
	{
		display_tx_cmd((uint8_t)(SH1107_CMD_SET_PAGE_ADDR + page),
		               (uint8_t[]){SH1107_CMD_SET_COL_HIGHER, SH1107_CMD_SET_COL_LOWER}, 2);
		// display_tx_cmd(SH1107_CMD_SET_COL_HIGHER, NULL, 0);
		// display_tx_cmd(SH1107_CMD_SET_COL_LOWER, NULL, 0);

		display_tx_data(&buffer[page * SH1107_WIDTH], SH1107_WIDTH);
	}
}
