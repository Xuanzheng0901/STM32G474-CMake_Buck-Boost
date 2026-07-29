/**
 * @file    ADC.c
 * @brief   ADC 硬件配置 + DMA 数据搬运, 控制逻辑已迁至 ctrl_loop
 */

#include "FreeRTOS.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "opamp.h"
#include "queue.h"
#include "ctrl_loop.h"

static uint32_t ac_adc_buf[2];
static uint32_t dc_adc_buf[ADC_BUFFER_LENGTH];

QueueHandle_t dc_adc_queue;

/* ---- ADC12 交流侧 ISR: 逐周期控制 (20kHz) ---- */

static inline void ac_isr(uint32_t adc_word)
{
    ctrl_loop_ac_isr(adc_word);
}

void ADC1_half_cplt_isr(ADC_HandleTypeDef *hadc)
{
    ac_isr(ac_adc_buf[0]);
}

void ADC1_cplt_isr(ADC_HandleTypeDef *hadc)
{
    ac_isr(ac_adc_buf[1]);
}

/* ---- ADC34 直流侧 ISR: 发队列 ---- */

static inline void dc_isr(uint16_t offset)
{
    BaseType_t woke = pdFALSE;
    const uint32_t *ptr = &dc_adc_buf[offset];
    xQueueSendFromISR(dc_adc_queue, &ptr, &woke);
    portYIELD_FROM_ISR(woke);
}

void ADC34_half_cplt_isr(ADC_HandleTypeDef *hadc)
{
    dc_isr(0);
}

void ADC34_cplt_isr(ADC_HandleTypeDef *hadc)
{
    dc_isr(ADC_BUFFER_LENGTH / 2);
}

/* ---- 初始化 ---- */

void ADC_init(void)
{
    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_Start(&hopamp3);

    HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
    HAL_DAC_Start(&hdac3, DAC_CHANNEL_2);
    dc_adc_queue = xQueueCreate(5, sizeof(uint32_t));

    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_HALF_CB_ID, ADC1_half_cplt_isr);
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, ADC1_cplt_isr);

    HAL_ADC_RegisterCallback(&hadc3, HAL_ADC_CONVERSION_HALF_CB_ID, ADC34_half_cplt_isr);
    HAL_ADC_RegisterCallback(&hadc3, HAL_ADC_CONVERSION_COMPLETE_CB_ID, ADC34_cplt_isr);

    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);

    HAL_ADCEx_MultiModeStart_DMA(&hadc1, ac_adc_buf, 2);
    HAL_ADCEx_MultiModeStart_DMA(&hadc3, dc_adc_buf, ADC_BUFFER_LENGTH);

    LOGI("ADC", "已启动");
}

QueueHandle_t ADC_get_dc_queue(void)
{
    return dc_adc_queue;
}
