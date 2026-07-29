/**
 * @file    pfc_utils.h
 * @brief   PFC 工具函数: 占空比计算、极性换向、HRTIM 寄存器写入
 */

#ifndef __PFC_UTILS_H__
#define __PFC_UTILS_H__

#include "arm_math.h"
#include "pfc_config.h"

/* ==================== PFC 工具函数声明 ==================== */

float32_t pfc_calc_ideal_duty(float32_t vin_inst, float32_t vout);

void pfc_write_duty(float32_t duty, uint8_t polarity);

void pfc_fast_bridge_disable(void);

void pfc_fast_bridge_enable(void);

void pfc_set_polarity(uint8_t polarity);

void pfc_slow_bridge_disable(void);

void pfc_power_stage_disable(void);

static inline float32_t pfc_gen_i_ref(float32_t i_amplitude, float32_t abs_sin)
{
    return i_amplitude * abs_sin;
}

#endif /* __PFC_UTILS_H__ */
