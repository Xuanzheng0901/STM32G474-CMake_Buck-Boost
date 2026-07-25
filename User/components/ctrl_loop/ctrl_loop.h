/**
 * @file    ctrl_loop.h
 * @brief   图腾柱 PFC 总控制回路 API
 *
 * @details 编排电流内环 (30kHz ISR) + 电压外环 (~300Hz 任务) + 软启动
 *
 * 使用方式:
 *   1. ctrl_loop_init()           — 系统初始化时调用一次
 *   2. ctrl_loop_current_isr()    — 每个 ADC 采样中断调用 (30kHz, 电流内环)
 *   3. ctrl_loop_voltage_task()   — FreeRTOS 任务中周期调用 (电压外环)
 */

#ifndef __CTRL_LOOP_H__
#define __CTRL_LOOP_H__

#include "arm_math.h"

/* ==================== 控制回路状态枚举 ==================== */

typedef enum {
    CTRL_STATE_IDLE = 0,        /**< 未启动 */
    CTRL_STATE_SOFT_START,      /**< 软启动: Vref 逐步上升 */
    CTRL_STATE_RUNNING,         /**< 正常运行 */
    CTRL_STATE_FAULT            /**< 故障保护 */
} CtrlLoop_State;

/* ==================== 公开 API ==================== */

/**
 * @brief  初始化 PFC 控制回路
 * @note   配置电流/电压 PI 参数, 进入软启动状态
 */
void ctrl_loop_init(void);

/**
 * @brief  获取当前控制状态
 */
CtrlLoop_State ctrl_loop_get_state(void);

/**
 * @brief  AC 采样 ISR 入口 (由 ADC 回调调用, 30kHz)
 * @param  adc_word : ADC12 双通道 32bit 字 (低12bit=电压, 高16bit=电流)
 * @note   内部依次: 归一化 → SOGI-PLL → 电流内环
 */
void ctrl_loop_ac_isr(uint32_t adc_word);

/**
 * @brief  电流内环 (每 ADC 采样调用一次, ~30kHz)
 * @param  vin_inst : 输入电压瞬时值 (归一化)
 * @param  il_inst  : 电感电流瞬时值 (归一化)
 * @param  vout     : 输出电压 (归一化, 来自慢速 ADC 或滤波值)
 * @param  abs_cos  : |cosθ|, SOGI 输出 (cos 与电网同相)
 * @param  polarity : 电网极性, 0=正半周, 1=负半周 (cosθ > 0 ? 0 : 1)
 */
void ctrl_loop_current_isr(float32_t vin_inst, float32_t il_inst,
                           float32_t vout,
                           float32_t abs_cos, uint8_t polarity);

/**
 * @brief  电压外环 (FreeRTOS 任务中周期调用, ~300Hz)
 * @param  vout_measured : 当前测量的输出电压 (归一化)
 * @note   内部自动处理: 软启动 Vref 斜坡 → 电压 PI → 更新电流参考幅值
 */
void ctrl_loop_voltage_task(float32_t vout_measured);

/**
 * @brief  获取当前电流参考幅值 (用于调试/显示)
 */
float32_t ctrl_loop_get_i_amplitude(void);

/**
 * @brief  获取当前工作占空比 (用于调试/显示)
 */
float32_t ctrl_loop_get_duty(void);

/**
 * @brief  设置目标输出电压
 * @param  vout : 目标电压 (V)
 * @note   可在运行时调用, 内部自动触发斜坡过渡
 */
void ctrl_loop_set_vout(float32_t vout);

/**
 * @brief  获取当前目标电压 (斜坡值, 用于显示)
 */
float32_t ctrl_loop_get_vref(void);

/**
 * @brief  获取滤波后的输出电压 (V), 供 UI 显示
 */
float32_t ctrl_loop_get_voltage(void);

/**
 * @brief  获取滤波后的输出电流 (A), 供 UI 显示
 */
float32_t ctrl_loop_get_current(void);

/**
 * @brief  获取 Vout 缓存值 (V), 供 ADC ISR 中电流环使用
 */
float32_t ctrl_loop_get_vout_cached(void);

void ctrl_loop_set_vout_cache(float32_t vout);

/**
 * @brief  设置慢管换向相位偏移
 * @param  deg : 偏移角度 (度), 正=提前, 负=延后, 默认 0
 */
void ctrl_loop_set_polarity_offset(float32_t deg);

void ctrl_loop_set_current_pi(float32_t kp, float32_t ki);
void ctrl_loop_set_voltage_pi(float32_t kp, float32_t ki);

#endif /* __CTRL_LOOP_H__ */
