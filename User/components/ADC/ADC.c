#include "stdio.h"
#include "FreeRTOS.h"
#include "adc.h"
#include "dma.h"
#include "queue.h"
#include "task.h"

#define ADC_BUFFER_LENGTH 50

static uint32_t adc_buffer_origin[ADC_BUFFER_LENGTH];
static const uint32_t *current_buffer = NULL;
TaskHandle_t adc_task_handle = NULL;
QueueHandle_t adc_queue = NULL;
static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    const uint32_t *ptr = &adc_buffer_origin[0];
    xQueueSendFromISR(adc_queue, &ptr, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    const uint32_t *ptr = &adc_buffer_origin[ADC_BUFFER_LENGTH / 2];
    xQueueSendFromISR(adc_queue, &ptr, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ADC_read_task(void *pvParameters)
{
    static uint32_t *buf_ptr;
    adc_queue = xQueueCreate(5, sizeof(uint32_t));
    HAL_ADC_Start(&hadc2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, adc_buffer_origin, ADC_BUFFER_LENGTH);
    while(1)
    {
        if(xQueueReceive(adc_queue, &buf_ptr, portMAX_DELAY) == pdTRUE)
        {
            double voltage_origin_summary = 0;
            double current_origin_summary = 0;
            for(uint8_t i = 0; i < 25; i++)
            {
                voltage_origin_summary += (uint16_t)buf_ptr[i];
                current_origin_summary += buf_ptr[i] >> 16;
            }
            voltage_origin_summary = voltage_origin_summary / 25.0 * 3300.0 / 4095.0 * 1.00516; //单位mV
            current_origin_summary = current_origin_summary / 25.0 * 3300.0 / 4095.0 * 1.00516; //单位mV
            // for (uint32_t i = 0; i < 5; i++)
            // {
            //     uint32_t adc_value = current_buffer[i];
            //     printf("ADC Value %lu: %ld mV\r\n", i, (uint32_t)((double)adc_value * 3300.0 / 4095.0 * 1.00516));
            // }
        }
    }
}
