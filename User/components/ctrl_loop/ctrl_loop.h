/**
 * @file    ctrl_loop.h
 * @brief   图腾柱 PFC 总控制回路 API
 *
 * @details 编排电流内环 (30kHz ISR) + 电压外环 (~300Hz 任务) + 软启动
 */

#ifndef __CTRL_LOOP_H__
#define __CTRL_LOOP_H__

#include "arm_math.h"
#include "ctrl_loop_state.h"

/* ==================== 公开 API ==================== */

void ctrl_loop_init(void);
CtrlLoop_State ctrl_loop_get_state(void);

/* AC 采样 ISR 入口 (30kHz, 由 ADC 回调调用) */
void ctrl_loop_ac_isr(uint32_t adc_word);

/* 电流内环 (30kHz ISR) */
void ctrl_loop_current_isr(float32_t vin_inst, float32_t il_inst,
                           float32_t vout, float32_t abs_cos, uint8_t polarity);

/* 设置目标输出电压, 自动触发软启动过渡 */
void ctrl_loop_set_vout(float32_t vout);

/* ---- 调试/显示 getter ---- */
float32_t ctrl_loop_get_vref(void);
float32_t ctrl_loop_get_voltage(void);
float32_t ctrl_loop_get_current(void);
float32_t ctrl_loop_get_i_amplitude(void);
float32_t ctrl_loop_get_duty(void);

/* 运行时调谐 PI 参数 */
void ctrl_loop_set_current_pi(float32_t kp, float32_t ki);
void ctrl_loop_set_voltage_pi(float32_t kp, float32_t ki, float32_t kd);

/* 相位补偿偏移 (度) */
void ctrl_loop_set_polarity_offset(float32_t deg);

/* 故障清除, 重新软启动 */
void ctrl_loop_clear_fault(void);

#endif /* __CTRL_LOOP_H__ */
