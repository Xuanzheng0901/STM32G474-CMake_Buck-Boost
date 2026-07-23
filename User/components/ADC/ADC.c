/**
 * @file    ADC.c
 * @brief   ADC 硬件配置 + DMA 数据搬运, 所有控制逻辑已迁至 ctrl_loop
 */

#include "FreeRTOS.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "opamp.h"
#include "queue.h"
#include "ctrl_loop.h"

static uint32_t ac_adc_buffer_origin[2];
static uint32_t dc_adc_buffer_origin[ADC_BUFFER_LENGTH];

QueueHandle_t dc_adc_queue = NULL;       /* ADC34 直流侧队列 */

/* ---- ADC12 注册回调: 纯实时控制 (30kHz, 不在 ISR 中发队列) ---- */

void ADC1_half_cplt_isr(ADC_HandleTypeDef *hadc)
{
    GPIOC->ODR ^= GPIO_PIN_1;
    ctrl_loop_ac_isr(ac_adc_buffer_origin[0]);   /* SOGI + 电流内环 */
    GPIOC->ODR ^= GPIO_PIN_1;
}

void ADC1_cplt_isr(ADC_HandleTypeDef *hadc)
{
    GPIOC->ODR ^= GPIO_PIN_1;
    ctrl_loop_ac_isr(ac_adc_buffer_origin[1]);   /* SOGI + 电流内环 */
    GPIOC->ODR ^= GPIO_PIN_1;
}

/* ---- ADC34 直流侧 ISR: 仅发队列 ---- */

void ADC34_half_cplt_isr(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const uint32_t *ptr = &dc_adc_buffer_origin[0];
    xQueueSendFromISR(dc_adc_queue, &ptr, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ADC34_cplt_isr(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const uint32_t *ptr = &dc_adc_buffer_origin[ADC_BUFFER_LENGTH / 2];
    xQueueSendFromISR(dc_adc_queue, &ptr, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---- 初始化 ---- */

void ADC_init(void)
{
    HAL_OPAMP_Start(&hopamp1);
    if(HAL_OPAMP_GetState(&hopamp1) != HAL_OPAMP_STATE_BUSY)
    {
        LOGI("ADC", "OPAMP已启动, 状态: %d", HAL_OPAMP_GetState(&hopamp1));
    }
    HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
    dc_adc_queue = xQueueCreate(5, sizeof(uint32_t));

    /* ADC12: 交流侧 V+I, 30kHz 逐周期控制 */
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_HALF_CB_ID, ADC1_half_cplt_isr);
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, ADC1_cplt_isr);

    /* ADC34: 直流侧 Vout+Iout, 150Hz → 队列 → ctrl_loop 任务 */
    HAL_ADC_RegisterCallback(&hadc3, HAL_ADC_CONVERSION_HALF_CB_ID, ADC34_half_cplt_isr);
    HAL_ADC_RegisterCallback(&hadc3, HAL_ADC_CONVERSION_COMPLETE_CB_ID, ADC34_cplt_isr);

    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);

    HAL_ADCEx_MultiModeStart_DMA(&hadc1, ac_adc_buffer_origin, 2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc3, dc_adc_buffer_origin, ADC_BUFFER_LENGTH);

    LOGI("ADC", "已启动");
}
