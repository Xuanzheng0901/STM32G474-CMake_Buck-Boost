#include "stdio.h"
#include "FreeRTOS.h"
#include "adc.h"
#include "dma.h"
#include "queue.h"
#include "task.h"

static uint32_t adc_buffer_origin[ADC_BUFFER_LENGTH];
// static const uint32_t *current_buffer = NULL;
TaskHandle_t adc_task_handle = NULL;
QueueHandle_t adc_queue = NULL;
float adc_result[2];

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const uint32_t *ptr = &adc_buffer_origin[0];
    xQueueSendFromISR(adc_queue, &ptr, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const uint32_t *ptr = &adc_buffer_origin[ADC_BUFFER_LENGTH / 2];
    xQueueSendFromISR(adc_queue, &ptr, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ADC_init(void)
{
    adc_queue = xQueueCreate(5, sizeof(uint32_t));
    HAL_ADC_Start(&hadc2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, adc_buffer_origin, ADC_BUFFER_LENGTH);
}
