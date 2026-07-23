/**
 * @file    pfc_utils.h
 * @brief   PFC 工具函数: 占空比计算、极性换向、HRTIM 寄存器写入
 *
 * @note    提供轻量内联 PI 控制器, 用于 30kHz 电流环
 */

#ifndef __PFC_UTILS_H__
#define __PFC_UTILS_H__

#include "arm_math.h"
#include "pfc_config.h"

/* ==================== 轻量 PI 控制器 ==================== */
/** @brief PI 控制器结构 (用于电流环/电压环) */
typedef struct {
    float32_t kp;           /**< 比例系数 */
    float32_t ki;           /**< 积分系数 (含 Ts, 调用方在 init 时预乘) */
    float32_t integral;     /**< 积分累加值 */
    float32_t int_max;      /**< 积分上限 */
    float32_t int_min;      /**< 积分下限 */
    float32_t out_max;      /**< 输出上限 */
    float32_t out_min;      /**< 输出下限 */
} PI_Controller;

/**
 * @brief  初始化 PI 控制器
 */
static inline void pi_init(PI_Controller *pi,
                           float32_t kp, float32_t ki,
                           float32_t int_max, float32_t int_min,
                           float32_t out_max, float32_t out_min)
{
    pi->kp       = kp;
    pi->ki       = ki;
    pi->integral = 0.0f;
    pi->int_max  = int_max;
    pi->int_min  = int_min;
    pi->out_max  = out_max;
    pi->out_min  = out_min;
}

/**
 * @brief  执行一次 PI 计算
 * @param  pi    : PI 控制器指针
 * @param  error : 当前误差 (参考 - 反馈)
 * @return PI 输出值 (已限幅)
 */
static inline float32_t pi_compute(PI_Controller *pi, float32_t error)
{
    pi->integral += pi->ki * error;
    if (pi->integral > pi->int_max) pi->integral = pi->int_max;
    if (pi->integral < pi->int_min) pi->integral = pi->int_min;

    float32_t out = pi->kp * error + pi->integral;
    if (out > pi->out_max) out = pi->out_max;
    if (out < pi->out_min) out = pi->out_min;

    return out;
}

/**
 * @brief  复位 PI 控制器 (清零积分, 其他参数不变)
 */
static inline void pi_reset(PI_Controller *pi)
{
    pi->integral = 0.0f;
}

/* ==================== PFC 工具函数声明 ==================== */

/**
 * @brief  计算理想 Boost 占空比
 * @param  vin_inst : 输入电压瞬时值 (归一化后)
 * @param  vout     : 输出电压 (归一化后)
 * @return 占空比 (0 ~ 0.95), 已限幅
 * @note   D = 1 - Vin / Vout, 忽略电感电阻
 */
float32_t pfc_calc_ideal_duty(float32_t vin_inst, float32_t vout);

/**
 * @brief  写入 Timer A 占空比到 HRTIM CMP3 寄存器
 * @param  duty : 占空比 (0.0 ~ 1.0)
 * @note   CMP1 固定为 0 (周期开始时置位), CMP3 = duty * Period (复位点)
 *         由于 Preload 使能, 新值在下个 PWM 周期生效
 */
void pfc_write_duty(float32_t duty);

/**
 * @brief  设置工频桥臂 (Timer B) 的导通状态
 * @param  polarity : 0 = 正半周 (下管 ON), 1 = 负半周 (上管 ON)
 * @note   硬件自动生成互补信号 + 死区, 只需写入 TB1 的占空比:
 *         正半周: TB1 = 0% (下管通)
 *         负半周: TB1 = 100% (上管通)
 */
void pfc_set_polarity(uint8_t polarity);

/**
 * @brief  生成瞬时电流参考值
 * @param  i_amplitude : 电流幅值 (来自电压外环)
 * @param  abs_sin     : |sin(θ)| (来自 SOGI-PLL)
 * @return Iref 瞬时值
 */
static inline float32_t pfc_gen_i_ref(float32_t i_amplitude, float32_t abs_sin)
{
    return i_amplitude * abs_sin;
}

#endif /* __PFC_UTILS_H__ */
