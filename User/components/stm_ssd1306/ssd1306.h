#ifndef G474_1_SSD1306_SPI_H
#define G474_1_SSD1306_SPI_H
#include <stdint.h>

void ssd1306_init();

void ssd1306_draw(uint8_t *color_data);

#endif
