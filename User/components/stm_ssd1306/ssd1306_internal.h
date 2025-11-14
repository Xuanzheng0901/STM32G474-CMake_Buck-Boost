#ifndef G474_1_SSD1306_INTERNAL_H
#define G474_1_SSD1306_INTERNAL_H

#include "spi.h"
#include "stdio_ext.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"

#define SSD1306_CMD_SET_MEMORY_ADDR_MODE  0x20
#define SSD1306_CMD_SET_COLUMN_RANGE      0x21
#define SSD1306_CMD_SET_PAGE_RANGE        0x22
#define SSD1306_CMD_SET_CHARGE_PUMP       0x8D
#define SSD1306_CMD_MIRROR_X_OFF          0xA0
#define SSD1306_CMD_MIRROR_X_ON           0xA1
#define SSD1306_CMD_INVERT_OFF            0xA6
#define SSD1306_CMD_INVERT_ON             0xA7
#define SSD1306_CMD_SET_MULTIPLEX         0xA8
#define SSD1306_CMD_DISP_OFF              0xAE
#define SSD1306_CMD_DISP_ON               0xAF
#define SSD1306_CMD_MIRROR_Y_OFF          0xC0
#define SSD1306_CMD_MIRROR_Y_ON           0xC8
#define SSD1306_CMD_SET_COMPINS           0xDA

#define SSD1306_HEIGHT 64
#define SSD1306_WIDTH 128

#define FULL_FLUSH_BUFFER_SIZE  SSD1306_WIDTH * SSD1306_HEIGHT / 8

#define SSD1306_USE_SPI
//#define SSD1306_USE_IIC

typedef enum {
    ssd1306_COMMAND = 0,
    ssd1306_DATA
} ssd1306_DC;


void ssd1306_reset(void);

void ssd1306_tx_cmd(uint8_t cmd, const uint8_t *param, size_t param_size);

void ssd1306_tx_data(uint8_t *data, size_t size);

#endif
