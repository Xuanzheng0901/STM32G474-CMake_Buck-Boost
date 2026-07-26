#include "stdio.h"
#include "FreeRTOS.h"
#include "adc.h"
#include "dma.h"
#include "queue.h"
#include "task.h"
#include "PID.h"

static adc_buf_t adc_buffer;
adc_dma_block_t block;
// static const uint32_t *current_buffer = NULL;
TaskHandle_t adc_task_handle = NULL;
QueueHandle_t adc_queue = NULL;
float adc_result[2];

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if(hadc != &hadc1)
        return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    block.adc12_half = &adc_buffer.adc12_buf[0];
    block.adc3_half = &adc_buffer.adc3_buf[0];
    xQueueSendFromISR(adc_queue, &block, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if(hadc != &hadc1)
        return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    block.adc12_half = &adc_buffer.adc12_buf[ADC_BUFFER_LENGTH / 2];
    block.adc3_half = &adc_buffer.adc3_buf[ADC_BUFFER_LENGTH / 2];
    xQueueSendFromISR(adc_queue, &block, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ADC_init(void)
{
    adc_queue = xQueueCreate(5, sizeof(adc_dma_block_t));
    // HAL_ADC_Start(&hadc2); // ADC2的 Overrun Behavior 必须配置为 Overwritten
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, adc_buffer.adc12_buf, ADC_BUFFER_LENGTH);
    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc_buffer.adc3_buf, ADC_BUFFER_LENGTH);
    LOGI("ADC", "已启动");
}
