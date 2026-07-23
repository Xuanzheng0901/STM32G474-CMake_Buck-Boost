#include "stdio.h"
#include "FreeRTOS.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "opamp.h"
#include "queue.h"
#include "sogi.h"
#include "task.h"
#include "ctrl_loop.h"

extern SPLL_1PH_SOGI spll;

static uint32_t adc_buffer_origin[2];
TaskHandle_t adc_task_handle = NULL;
QueueHandle_t adc_queue = NULL;
float adc_result[2];

/* 缓存的输出电压 (由电压环任务更新) */
static float32_t vout_cached = 0.0f;
void adc_set_vout_cache(float32_t vout) { vout_cached = vout; }

/* 内部: 处理单个 ADC 采样点 */
static inline void adc_process_sample(uint32_t adc_word)
{
    /* 提取 ADC1 电压 (低 12bit) 和 ADC2 电流 (高 16bit) */
    int32_t  v_raw   = (int32_t)(adc_word & 0x0FFF);
    int32_t  i_raw   = (int32_t)(adc_word >> 16);

    /* 归一化: (raw - offset) / scale */
    float32_t v_inst = (float32_t)(v_raw - 2048) / 1365.33f;
    float32_t i_inst = (float32_t)(i_raw - 2048) / 1365.33f;

    /* SOGI-PLL: 更新电网相位 */
    SPLL_1PH_SOGI_run(&spll, v_inst);

    /* 极性: sinθ > 0 → 正半周 (极性=0), sinθ < 0 → 负半周 (极性=1) */
    uint8_t polarity = (spll.sine >= 0.0f) ? 0 : 1;
    float32_t abs_sin = (spll.sine >= 0.0f) ? spll.sine : -spll.sine;

    /* PFC 电流内环 */
    ctrl_loop_current_isr(v_inst, i_inst, vout_cached, abs_sin, polarity);
}

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
    adc_process_sample(adc_buffer_origin[0]);
    GPIOC->ODR ^= GPIO_PIN_1;
}

void ADC1_cplt_isr(ADC_HandleTypeDef *hadc)
{
    GPIOC->ODR ^= GPIO_PIN_1;
    adc_process_sample(adc_buffer_origin[1]);
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
