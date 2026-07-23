#include "stdio.h"
#include "FreeRTOS.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "opamp.h"
#include "queue.h"
#include "sogi.h"
#include "task.h"

extern SPLL_1PH_SOGI spll;

static uint32_t adc_buffer_origin[ADC_BUFFER_LENGTH];
// static const uint32_t *current_buffer = NULL;
TaskHandle_t adc_task_handle = NULL;
QueueHandle_t adc_queue = NULL;
float adc_result[2];

uint8_t count = 0;

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

void ADC1_half_cplt_isr(ADC_HandleTypeDef *hadc)
{
    GPIOC->ODR ^= GPIO_PIN_1;
    float32_t vNorm = (float32_t)(((int32_t)(adc_buffer_origin[0] & 0x0FFF)) - 2048) / 1365.33f;
    SPLL_1PH_SOGI_run(&spll, vNorm);
    // if(++count >= 2)
    // {
    HAL_DAC_SetValue(&hdac3,
                     DAC_CHANNEL_1,
                     DAC_ALIGN_12B_R,
                     (spll.cosine + 1) * 2047);
    //     count = 0;
    // }

    GPIOC->ODR ^= GPIO_PIN_1;
}

void ADC1_cplt_isr(ADC_HandleTypeDef *hadc)
{
    GPIOC->ODR ^= GPIO_PIN_1;
    float32_t vNorm = (float32_t)(((int32_t)(adc_buffer_origin[1] & 0x0FFF)) - 2048) / 1365.33f;
    SPLL_1PH_SOGI_run(&spll, vNorm);
    // if(++count >= 2)
    // {
    HAL_DAC_SetValue(&hdac3,
                     DAC_CHANNEL_1,
                     DAC_ALIGN_12B_R,
                     (spll.cosine + 1) * 2047);
    count = 0;
    // }
    GPIOC->ODR ^= GPIO_PIN_1;
}

void ADC_init(void)
{
    HAL_OPAMP_Start(&hopamp1);
    if(HAL_OPAMP_GetState(&hopamp1) != HAL_OPAMP_STATE_BUSY)
    {
        LOGI("ADC", "OPAMP已启动, 状态: %d", HAL_OPAMP_GetState(&hopamp1));
    }
    HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
    adc_queue = xQueueCreate(5, sizeof(uint32_t));
    // HAL_ADC_Start(&hadc2); // ADC2的 Overrun Behavior 必须配置为 Overwritten
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_HALF_CB_ID, ADC1_half_cplt_isr);
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, ADC1_cplt_isr);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, adc_buffer_origin, ADC_BUFFER_LENGTH);


    LOGI("ADC", "已启动");
}
