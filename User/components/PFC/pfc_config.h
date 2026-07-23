/**
 * @file    pfc_config.h
 * @brief   图腾柱无桥 PFC 硬件参数配置
 *
 * @note    根据实际硬件修改以下参数后再编译
 */

#ifndef __PFC_CONFIG_H__
#define __PFC_CONFIG_H__

#include "arm_math.h"
#include "main.h"

/* ==================== PWM 参数 ==================== */
#define PFC_PWM_PERIOD          PWM_Period       /**< HRTIM 周期 (168MHz*8/44800 = 30kHz) */
#define PFC_PWM_FREQ            30000.0f     /**< PWM 开关频率 (Hz) */
#define PFC_DUTY_MIN            0.02f        /**< 最小占空比 (防止脉冲丢失) */
#define PFC_DUTY_MAX            0.95f        /**< 最大占空比 (留死区余量) */

/* ==================== 功率电路参数 ==================== */
#define PFC_INDUCTOR_UH         1000.0f      /**< 升压电感 (uH), 按实际值修改 */
#define PFC_CAPACITOR_UF        1000.0f      /**< 输出电容 (uF), 按实际值修改 */
#define PFC_NOMINAL_VIN_RMS     12.0f        /**< 额定输入电压 RMS (V) */
#define PFC_NOMINAL_VOUT        24.0f        /**< 额定输出电压 DC (V) */
#define PFC_MAX_CURRENT_A       3.0f         /**< 最大输入电流 RMS (A) */

/* ==================== ADC 采样参数 ==================== */
#define PFC_ADC_VREF            3.0f         /**< ADC 参考电压 (V) */
#define PFC_ADC_RESOLUTION      4096.0f      /**< ADC 量程 (12bit) */
#define PFC_ADC_OFFSET          2048.0f      /**< ADC 零点偏置 (1.5V 对应码值) */

/* ==================== 控制参数 ==================== */
#define PFC_VOLTAGE_LOOP_DIV    100U         /**< 电压环分频: 每 N 个电流环执行一次 (300Hz) */
#define PFC_SOFTSTART_STEPS     200U         /**< 软启动步数 */
#define PFC_SOFTSTART_STEP_MS   5U           /**< 软启动每步时长 (ms) */

/* ==================== PI 默认参数 ==================== */
/* 电流内环 (30kHz), 带宽 ~2kHz */
#define PFC_I_KP_DEFAULT        0.1f
#define PFC_I_KI_DEFAULT        0.02f
#define PFC_I_INTEGRAL_MAX      0.5f
#define PFC_I_OUTPUT_MAX        0.5f

/* 电压外环 (~300Hz), 带宽 ~15Hz */
#define PFC_V_KP_DEFAULT        0.02f
#define PFC_V_KI_DEFAULT        0.002f
#define PFC_V_INTEGRAL_MAX      3.0f
#define PFC_V_OUTPUT_MAX        3.0f

#endif /* __PFC_CONFIG_H__ */
