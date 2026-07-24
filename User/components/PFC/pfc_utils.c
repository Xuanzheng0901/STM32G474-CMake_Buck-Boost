/**
 * @file    pfc_utils.c
 * @brief   PFC 工具函数实现
 */

#include "pfc_utils.h"
#include "hrtim.h"

/* ==================== 占空比计算 ==================== */

float32_t pfc_calc_ideal_duty(float32_t vin_inst, float32_t vout)
{
    /*
     * 两个交流半周共用同一个 Boost 前馈关系，因此这里必须使用
     * 电网电压的幅值。低频桥臂的极性换向由 pfc_set_polarity() 处理。
     */
    float32_t vin_abs = (vin_inst >= 0.0f) ? vin_inst : -vin_inst;

    /* D_boost = 1 - |Vin| / Vout  (理想 Boost 关系) */
    if (vout < 0.1f) return PFC_DUTY_MAX; /* Vout 过低, 保护 */

    float32_t duty = 1.0f - vin_abs / vout;

    /* 限幅 */
    if (duty > PFC_DUTY_MAX) duty = PFC_DUTY_MAX;
    if (duty < PFC_DUTY_MIN) duty = PFC_DUTY_MIN;

    return duty;
}

/* ==================== HRTIM 寄存器写入 ==================== */

/*
 * TA1 输出配置 (CubeMX):
 *   Set   = PERIOD   → 周期末置高
 *   Reset = CMP1     → CMP1 处复位
 *   ON  = [0, CMP1],  OFF = [CMP1, Period]
 *   CMP3 空闲 → 用作 ADC 触发点 (较长半周中点, 远离开关沿)
 */
void pfc_write_duty(float32_t duty)
{
    if (duty > PFC_DUTY_MAX) duty = PFC_DUTY_MAX;
    if (duty < PFC_DUTY_MIN) duty = PFC_DUTY_MIN;

    uint32_t period = PFC_PWM_PERIOD;
    uint32_t cmp1   = (uint32_t)(duty * (float32_t)period);

    /* ADC 采样点: 较长 PWM 半周的中点, 避开开关噪声 */
    uint32_t cmp3;
    if (cmp1 <= period / 2) {
        /* duty ≤ 50%, 较长半周 = OFF [CMP1, Period], 中点 = (CMP1+Period)/2 */
        cmp3 = (cmp1 + period) / 2;
    } else {
        /* duty > 50%, 较长半周 = ON [0, CMP1], 中点 = CMP1/2 */
        cmp3 = cmp1 / 2;
    }

    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = cmp1;
    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP3xR = cmp3;
}

/*
 * TB1: CubeMX 设 Set=NONE, Reset=NONE.
 * 直接软件强制输出电平, 不经过比较器, 无 Set/Reset 重合问题.
 * TB2 由硬件自动反相, 无需软件控制.
 */
void pfc_set_polarity(uint8_t polarity)
{
    if (polarity == 0)
    {
        /* 正半周: TB1 强制 LOW → 硬件反相后 TB2=HIGH (下管导通回流) */
        HAL_HRTIM_WaveformSetOutputLevel(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B,
                                         HRTIM_OUTPUT_TB1, HRTIM_OUTPUTLEVEL_INACTIVE);
    }
    else
    {
        /* 负半周: TB1 强制 HIGH → 硬件反相后 TB2=LOW (上管导通回流) */
        HAL_HRTIM_WaveformSetOutputLevel(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B,
                                         HRTIM_OUTPUT_TB1, HRTIM_OUTPUTLEVEL_ACTIVE);
    }
}
