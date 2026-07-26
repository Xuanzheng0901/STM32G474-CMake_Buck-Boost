/**
 * @file    pfc_utils.c
 * @brief   PFC 工具函数实现
 */

#include "pfc_utils.h"
#include "hrtim.h"

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

/*
 * TB1: CubeMX 设 Set=CMP1, Reset=CMP3. TB2 硬件反相.
 * 0%: CMP1=1, CMP3=1 → Set/Reset 同拍, Reset 优先 → 常低
 * 100%: CMP1=1, CMP3=Period → Set 在 timer=1, Reset 在 Period → 常高
 */
void pfc_set_polarity(uint8_t polarity)
{
    if(polarity == 0)
    {
        HAL_HRTIM_WaveformSetOutputLevel(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB1,HRTIM_OUTPUTLEVEL_ACTIVE);
        HAL_HRTIM_WaveformSetOutputLevel(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB2,
                                         HRTIM_OUTPUTLEVEL_INACTIVE);
    }
    else
    {
        HAL_HRTIM_WaveformSetOutputLevel(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB1,
                                         HRTIM_OUTPUTLEVEL_INACTIVE);
        HAL_HRTIM_WaveformSetOutputLevel(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB2,
                                         HRTIM_OUTPUTLEVEL_ACTIVE);
    }
}
