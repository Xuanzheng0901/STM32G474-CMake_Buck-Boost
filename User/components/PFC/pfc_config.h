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
#define PFC_PWM_FREQ            20000.0f     /**< PWM 开关频率 (Hz) */
#define PFC_DUTY_MIN            0.02f        /**< 最小占空比 (防止脉冲丢失) */
#define PFC_DUTY_MAX            0.95f        /**< 最大占空比 (留死区余量) */

/* ==================== 功率电路参数 ==================== */
#define PFC_INDUCTOR_UH         1000.0f      /**< 升压电感 (uH), 按实际值修改 */
#define PFC_CAPACITOR_UF        1000.0f      /**< 输出电容 (uF), 按实际值修改 */
#define PFC_NOMINAL_VIN_RMS     20.0f        /**< 额定输入电压 RMS (V) */
#define PFC_NOMINAL_VOUT        40.0f        /**< 额定输出电压 DC (V) */
#define PFC_MAX_CURRENT_A       3.0f         /**< 最大输入电流 RMS (A) */

/* ==================== ADC 采样参数 ==================== */
/*
 * ADC1_IN1 (AC 电压): 衰减 43/1.1, 偏置 1.5V → 2048
 *   V_actual = (raw - 2048) / 34.93
 */
#define PFC_VIN_OFFSET          2048.0f
#define PFC_VIN_LSB_PER_V       34.93f        /**< 1V 对应 ADC LSB 数 */

/*
 * ADC2_IN8 (AC 电流): 132mV/A, 偏置 1.65V → 2253
 *   I_actual = (raw - 2253) / 180.2
 */
#define PFC_IIN_OFFSET          2253.0f
#define PFC_IIN_LSB_PER_A       180.2f        /**< 1A 对应 ADC LSB 数 */

/*
 * ADC3_IN6 (DC 电压): 衰减 20×, 参考 GND (无偏置)
 *   Vout = raw / 68.27
 */
#define PFC_VOUT_OFFSET         0.0f
#define PFC_VOUT_LSB_PER_V      68.27f

/*
 * ADC4_IN8 (DC 电流), 参考 GND
 */
#define PFC_IOUT_OFFSET         0.0f
#define PFC_IOUT_LSB_PER_A      PFC_IIN_LSB_PER_A

/* ==================== 控制参数 ==================== */
/**
 * 电压环频率 = (PWM频率) / (ADC34触发分频 × 半缓冲点数)
 *            = 20000 / (10 × 20) = 100 Hz
 */
#define PFC_VOLTAGE_LOOP_FREQ   100.0f       /**< 电压外环频率 (Hz) */
#define PFC_SOFTSTART_SEC       1.0f         /**< 软启动持续时间 (s) */

/* ==================== PI 默认参数 ==================== */
/* 电流内环 (30kHz), 带宽 ~2kHz */
#define PFC_I_KP_DEFAULT        0.2f
#define PFC_I_KI_DEFAULT        0.2f
#define PFC_I_INTEGRAL_MAX      2.0f
#define PFC_I_OUTPUT_MAX        0.5f

/* 电压外环 (~300Hz), 带宽 ~15Hz */
#define PFC_V_KP_DEFAULT        0.2f
#define PFC_V_KI_DEFAULT        0.8f
#define PFC_V_KD_DEFAULT        0.05f
#define PFC_V_INTEGRAL_MAX      10.0f
#define PFC_V_OUTPUT_MAX        3.0f

/* ==================== 保护参数 ==================== */
#define PFC_OVP_RATIO           1.2f        /**< 过压保护阈值 = Vout_target × ratio */
#define PFC_OCP_AMPS            3.5f        /**< 过流保护阈值 (A) */
#define PFC_UVP_VIN_RMS         5.0f        /**< 电网欠压阈值 RMS (V), 低于此值视为电网丢失 */
#define PFC_SS_TIMEOUT_SEC      5.0f        /**< 软启动超时 (s), 超时未达目标触发故障 */
#define PFC_FAULT_RETRY_MS      5000        /**< 故障后重试间隔 (ms) */

/* ==================== 调试选项 ==================== */
#define PFC_DEBUG_DAC_OUTPUT    1           /**< 设为 1 启用 ISR 中 DAC 调试输出 */

#endif /* __PFC_CONFIG_H__ */
