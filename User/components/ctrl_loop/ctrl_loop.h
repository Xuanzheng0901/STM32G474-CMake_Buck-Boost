/**
 * @file    ctrl_loop.h
 * @brief   图腾柱 PFC 总控制回路 API
 *
 * @details 编排电流内环 (20kHz ISR) + 电压外环 (100Hz 任务) + 软启动
 */

#ifndef __CTRL_LOOP_H__
#define __CTRL_LOOP_H__

#include <stdbool.h>

#include "arm_math.h"
#include "ctrl_loop_state.h"

/* ==================== 公开 API ==================== */

void ctrl_loop_init(void);
CtrlLoop_State ctrl_loop_get_state(void);

/* AC 采样 ISR 入口 (20kHz, 由 ADC 回调调用) */
void ctrl_loop_ac_isr(uint32_t adc_word);

/* 电流内环 (20kHz ISR)
 * ref_wave: -1~1 有符号正弦参考, polarity: 慢桥臂极性 */
void ctrl_loop_current_isr(float32_t vin_inst, float32_t il_inst, float32_t vout,
                           float32_t ref_wave, uint8_t polarity);

/* 设置目标输出电压, 自动触发软启动过渡 */
void ctrl_loop_set_vout(float32_t vout);

/* ---- 调试/显示 getter ---- */
float32_t ctrl_loop_get_vref(void);
float32_t ctrl_loop_get_voltage(void);
float32_t ctrl_loop_get_current(void);
float32_t ctrl_loop_get_input_voltage_rms(void);
float32_t ctrl_loop_get_i_amplitude(void);
float32_t ctrl_loop_get_duty(void);
PfcFaultReason ctrl_loop_get_fault_reason(void);

/* 运行时调谐电流准 PR / 电压 PI 参数 */
bool ctrl_loop_set_current_pr(float32_t kp, float32_t kr, float32_t bandwidth_hz);
void ctrl_loop_set_voltage_pi(float32_t kp, float32_t ki, float32_t kd);

/* 相位补偿偏移 (度) */
void ctrl_loop_set_polarity_offset(float32_t deg);

/* 故障清除, 重新软启动 */
void ctrl_loop_clear_fault(void);

#endif /* __CTRL_LOOP_H__ */
