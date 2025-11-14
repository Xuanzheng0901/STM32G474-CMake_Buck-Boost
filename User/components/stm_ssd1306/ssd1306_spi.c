#include "ssd1306_internal.h"

#ifdef SSD1306_USE_SPI

void ssd1306_reset(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    vTaskDelay(10);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    vTaskDelay(10);
}

void ssd1306_dc(ssd1306_DC dc)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, (GPIO_PinState)dc);
}

void ssd1306_tx_cmd(uint8_t cmd, const uint8_t *param, size_t param_size)
{
    ssd1306_dc(ssd1306_COMMAND);
    HAL_SPI_Transmit_DMA(&hspi1, &cmd, 1);
    if(param != NULL && param_size > 0)
    {
        HAL_SPI_Transmit_DMA(&hspi1, param, param_size);
    }
}

void ssd1306_tx_data(uint8_t *data, size_t size)
{
    ssd1306_dc(ssd1306_DATA);
    HAL_SPI_Transmit_DMA(&hspi1, data, size);
}

#endif
