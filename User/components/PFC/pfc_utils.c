/**
 * @file    pfc_utils.c
 * @brief   PFC 工具函数实现
 */

#include "pfc_utils.h"
#include "hrtim.h"
#include "main.h"

#define PFC_SLOW_BRIDGE_GPIO_PORT  GPIOA
#define PFC_SLOW_BRIDGE_POS_PIN    GPIO_PIN_10
#define PFC_SLOW_BRIDGE_NEG_PIN    GPIO_PIN_11
#define PFC_SLOW_BRIDGE_PIN_MASK   (PFC_SLOW_BRIDGE_POS_PIN | PFC_SLOW_BRIDGE_NEG_PIN)
#define PFC_FAST_BRIDGE_OUTPUTS    (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2)

/* ==================== 占空比计算 ==================== */

float32_t pfc_calc_ideal_duty(float32_t vin_inst, float32_t vout)
{
    float32_t vin_abs = (vin_inst >= 0.0f) ? vin_inst : -vin_inst;
    if(vout < 0.1f)
        return PFC_DUTY_MAX;
    float32_t duty = 1.0f - vin_abs / vout;
    if(duty > PFC_DUTY_MAX)
        duty = PFC_DUTY_MAX;
    if(duty < PFC_DUTY_MIN)
        duty = PFC_DUTY_MIN;
    return duty;
}

/* ==================== HRTIM 寄存器写入 ==================== */

/*
 * TA1 Set=PERIOD, Reset=CMP1 → TA1 ON=CMP1, TA2 硬件反相 ON=(Period-CMP1).
 * 正半周(polarity=0): TA2 为 boost 管 → 需 TA2 ON=D → CMP1=(1-D)*Period
 * 负半周(polarity=1): TA1 为 boost 管 → 需 TA1 ON=D → CMP1=D*Period
 */
void pfc_write_duty(float32_t duty, uint8_t polarity)
{
    if(duty > PFC_DUTY_MAX)
        duty = PFC_DUTY_MAX;
    if(duty < PFC_DUTY_MIN)
        duty = PFC_DUTY_MIN;

    uint32_t period = PFC_PWM_PERIOD;
    uint32_t cmp1;

    if(polarity == 0)
        cmp1 = (uint32_t)((1.0f - duty) * (float32_t)period);
    else
        cmp1 = (uint32_t)(duty * (float32_t)period);

    uint32_t cmp3;
    if(cmp1 <= period / 2)
        cmp3 = (cmp1 + period) / 2;
    else
        cmp3 = cmp1 / 2;

    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = cmp1;
    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP3xR = cmp3;
}

void pfc_fast_bridge_disable(void)
{
    hhrtim1.Instance->sCommonRegs.ODISR |= PFC_FAST_BRIDGE_OUTPUTS;
}

void pfc_fast_bridge_enable(void)
{
    hhrtim1.Instance->sCommonRegs.OENR |= PFC_FAST_BRIDGE_OUTPUTS;
}

void pfc_set_polarity(uint8_t polarity)
{
    uint32_t set_pin;
    uint32_t reset_pin;

    if(polarity == 0U) {
        set_pin = PFC_SLOW_BRIDGE_POS_PIN;
        reset_pin = PFC_SLOW_BRIDGE_NEG_PIN;
    }
    else {
        set_pin = PFC_SLOW_BRIDGE_NEG_PIN;
        reset_pin = PFC_SLOW_BRIDGE_POS_PIN;
    }

    /* 同一次 BSRR 写入完成一开一关，避免中间态。 */
    PFC_SLOW_BRIDGE_GPIO_PORT->BSRR = set_pin | (reset_pin << 16U);
}

void pfc_slow_bridge_disable(void)
{
    PFC_SLOW_BRIDGE_GPIO_PORT->BSRR =
        (uint32_t)PFC_SLOW_BRIDGE_PIN_MASK << 16U;
}

void pfc_power_stage_disable(void)
{
    pfc_fast_bridge_disable();
    pfc_slow_bridge_disable();
}
