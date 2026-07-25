/**
 * @file    sogi.h
 * @brief   SOGI-PLL (Second-Order Generalized Integrator) 单相软件锁相环
 * @details
 * ## 原理
 * 从单相电网电压中提取相位、频率和正交分量，由三级串联构成:
 *
 *     Vin ─→ [SOGI 正交信号发生器] ─→ [Park 变换 αβ→dq] ─→ [PI 环路滤波 + VCO] ─→ θ
 *              ↑                                                              │
 *              └────────────────── sinθ / cosθ ───────────────────────────────┘
 *
 * ## 使用方法
 * ```
 * // 1. 初始化
 * SPLL_1PH_SOGI spll;
 * SPLL_1PH_SOGI_reset(&spll);
 * SPLL_1PH_SOGI_config(&spll, 50.0f, 20000.0f, B0_LPF, B1_LPF);
 *
 * // 2. 每个 ISR 周期调用 (例: 20kHz ADC 中断)
 * float32_t vNorm  = (adcRaw - 2048) / 1365.33f;  // 调用方自行归一化
 * float32_t theta  = SPLL_1PH_SOGI_run(&spll, vNorm);
 * float32_t freq   = spll.fo;      // 锁定的电网频率
 * float32_t sinVal = spll.sine;    // sin(θ)
 * ```
 *
 * ## PI 参数计算公式
 * ```
 * Ts = 1 / isrFrequency
 * Kp = 2 * ζ * wn
 * Ki = wn²
 * b0 = Kp + Ki * Ts
 * b1 = -Kp
 * ```
 *
 * ## 参考
 * - https://www.cnblogs.com/swear/p/12682551.html (SOGI 离散化推导)
 * - "The natural frequency and the damping ratio of the linearized PLL"
 */

#ifndef __SOGI_H__
#define __SOGI_H__

#include "arm_math.h"

/** @brief 调试 DAC 输出使能 (1=输出 cosθ 到 DAC_CH1, 0=禁用) */
#define SOGI_DEBUG_DAC_OUTPUT   1

/** @brief SOGI 阻尼因子 k
 *  @details k 越大 → 带宽越宽、响应越快、抗谐波能力下降
 *           k 越小 → 选择性越好、响应变慢
 *           推荐范围 0.3 ~ 1.0, 默认 0.5 */
#define SOGI_K      0.5f

/** @brief 2π, 用于相位归一化 */
#define Pi_2        6.2831852f

/** @brief 电网额定频率 (Hz) */
#define AcFreq      50.0f

/** @brief ISR 调用频率 (Hz) */
#define IsrFrequency 20000.0f

/**
 * @brief PI 环路滤波器参数设计依据
 * @details
 * 自然频率 wn = 110 rad/s, 阻尼比 ζ = 0.7, 锁相带宽 ≈ 17.5 Hz.\n
 * 推导过程:
 *   Kp = 2 * ζ * wn = 154
 *   Ki = wn² = 12100
 *   Ts = 1/20000 = 5e-5
 *   b0 = Kp + Ki * Ts ≈ 154.6
 *   b1 = -Kp = -154
 * @note 输入信号幅值不为 ±1 时需归一化: b0 和 b1 除以 Um
 */
#define B0_LPF      154.00000f      /**< PI 控制器 b0 = Kp + Ki*Ts */
#define B1_LPF     -153.99998f      /**< PI 控制器 b1 = -Kp */
#define A1_LPF      1.0f            /**< PI 控制器 a1 (未使用, 预留) */

/* ================================================================
 *  数据结构定义
 * ================================================================ */

/**
 * @brief SOGI (二阶广义积分器) 离散化系数
 * @details
 * 双线性变换将连续域传递函数映射到离散域:\n
 *   x  = 2 * k * wn * Ts\n
 *   y  = (wn * Ts)²\n
 *   temp = 1 / (x + y + 4)\n
 *   b0 = x * temp,  b2 = -b0\n
 *   a1 = 2*(4-y)*temp,  a2 = (x - y - 4)*temp\n
 *   qb0 = k*y*temp,  qb1 = 2*qb0,  qb2 = qb0
 */
typedef struct {
    float32_t osg_k;    /**< SOGI 阻尼因子 k, 同 SOGI_K */
    float32_t osg_x;    /**< 中间变量 x = 2*k*wn*Ts */
    float32_t osg_y;    /**< 中间变量 y = (wn*Ts)² */
    float32_t osg_b0;   /**< H_d 分子系数 b0 (z² 项) */
    float32_t osg_b2;   /**< H_d 分子系数 b2 (常数项, = -b0) */
    float32_t osg_a1;   /**< H_d/H_q 分母系数 a1 (z 项) */
    float32_t osg_a2;   /**< H_d/H_q 分母系数 a2 (常数项) */
    float32_t osg_qb0;  /**< H_q 分子系数 qb0 (z² 项) */
    float32_t osg_qb1;  /**< H_q 分子系数 qb1 (z 项, = 2*qb0) */
    float32_t osg_qb2;  /**< H_q 分子系数 qb2 (常数项, = qb0) */
} SPLL_1PH_SOGI_OSG_COEFF;

/**
 * @brief PI 环路滤波器系数
 */
typedef struct {
    float32_t b1;       /**< 反馈系数, b1 = -Kp */
    float32_t b0;       /**< 前向系数, b0 = Kp + Ki*Ts */
    float32_t a1;       /**< 反馈系数 a1 (未使用, 预留) */
} SPLL_1PH_SOGI_LPF_COEFF;

/**
 * @brief SOGI-PLL 主结构体 (句柄)
 * @details 包含所有运行时状态、系数和锁相输出.\n
 *          全局实例 spll 在 sogi.c 中定义.
 */
typedef struct {
    /**@{ @name 输入缓冲 */
    float32_t u[3];       /**< 输入电压滑动窗口 [k, k-1, k-2] */
    /**@}*/

    /**@{ @name SOGI 正交信号输出 */
    float32_t osg_u[3];   /**< SOGI α轴 (同相分量) 滑动窗口 [k, k-1, k-2] */
    float32_t osg_qu[3];  /**< SOGI β轴 (正交分量) 滑动窗口 [k, k-1, k-2] */
    /**@}*/

    /**@{ @name Park 变换输出 (dq 旋转坐标系) */
    float32_t u_Q[2];     /**< Q轴分量 [k, k-1] (相位误差, 锁定时 → 0) */
    float32_t u_D[2];     /**< D轴分量 [k, k-1] (电压幅值) */
    /**@}*/

    /**@{ @name PI 环路滤波器 */
    float32_t ylf[2];     /**< PI 输出 [k, k-1] (频率修正量) */
    /**@}*/

    /**@{ @name 锁相输出 (每拍更新) */
    float32_t fo;         /**< 当前锁定的电网频率 (Hz) */
    float32_t fn;         /**< 额定频率 (Hz), 由 SPLL_1PH_SOGI_config 设置 */
    float32_t theta;      /**< 锁相角 (rad), 范围 0 ~ 2π */
    float32_t cosine;     /**< cos(theta), 供下一拍 Park 变换使用 */
    float32_t sine;       /**< sin(theta), 供下一拍 Park 变换使用 */
    /**@}*/

    float32_t delta_t;    /**< ISR 周期 = 1 / isrFrequency (s) */

    /**@{ @name 系数 */
    SPLL_1PH_SOGI_OSG_COEFF osg_coeff; /**< SOGI 离散化系数 */
    SPLL_1PH_SOGI_LPF_COEFF lpf_coeff; /**< PI 环路滤波器系数 */
    /**@}*/
} SPLL_1PH_SOGI;

/* ================================================================
 *  全局变量声明
 * ================================================================ */

/** @brief 全局 SOGI-PLL 实例 */
extern SPLL_1PH_SOGI spll;

/* ================================================================
 *  API 函数声明
 * ================================================================ */

/**
 * @brief  复位 SOGI-PLL, 清零所有内部状态
 * @param  spll_obj : PLL 实例指针
 * @note   复位后需重新调用 SPLL_1PH_SOGI_config 配置参数
 */
void SPLL_1PH_SOGI_reset(SPLL_1PH_SOGI *spll_obj);

/**
 * @brief  根据当前 fn 和 delta_t 计算 SOGI 离散化系数
 * @param  spll_obj : PLL 实例指针
 * @note   由 SPLL_1PH_SOGI_config 内部调用, 用户一般无需手动调用
 * @details
 * 双线性变换公式:\n
 *   x = 2 * k * wn * Ts\n
 *   y = (wn * Ts)²\n
 *   temp = 1 / (x + y + 4)\n
 *   差分方程: u_α[k] = b0·u[k] + b2·u[k-2] + a1·u_α[k-1] + a2·u_α[k-2]
 */
void SPLL_1PH_SOGI_coeff_calc(SPLL_1PH_SOGI *spll_obj);

/**
 * @brief  配置 SOGI-PLL 参数
 * @param  spll_obj     : PLL 实例指针
 * @param  acFreq       : 电网额定频率 (Hz), 中国 50Hz, 北美 60Hz
 * @param  isrFrequency : ISR 调用频率 (Hz), 需与实际中断频率一致
 * @param  lpf_b0       : PI 前向系数, b0 = Kp + Ki*Ts (默认 B0_LPF = 154)
 * @param  lpf_b1       : PI 反馈系数, b1 = -Kp (默认 B1_LPF = -154)
 * @note   调用前需先 SPLL_1PH_SOGI_reset
 * @note   本函数内部会调用 SPLL_1PH_SOGI_coeff_calc 计算 SOGI 系数
 */
void SPLL_1PH_SOGI_config(SPLL_1PH_SOGI *spll_obj,
                          float32_t acFreq,
                          float32_t isrFrequency,
                          float32_t lpf_b0,
                          float32_t lpf_b1);

/**
 * @brief  运行一帧锁相计算 (每个 ISR 周期调用一次)
 * @param  spll_obj : PLL 实例指针
 * @param  acValue  : 归一化后的电压瞬时值 (调用方自行完成 ADC 归一化)
 * @return 当前锁相角 theta (rad), 范围 [0, 2π)
 *
 * @details
 * ## 每拍执行流程
 *
 * #### 第 1 步: SOGI 正交信号生成
 *          u_α = b0·u[k] + b2·u[k-2] + a1·u_α[k-1] + a2·u_α[k-2]  (带通, 同相)
 *          u_β = qb0·u[k] + qb1·u[k-1] + qb2·u[k-2] + a1·u_β[k-1] + a2·u_β[k-2]  (低通, 正交)
 *          u_α 与 u_β 幅值相同, 相位差 90°
 *
 * #### 第 2 步: Park 变换 (αβ → dq)
 *          U_D = u_α * cosθ + u_β * sinθ  (D轴 = 电压幅值)
 *          U_Q = u_β * cosθ - u_α * sinθ  (Q轴 = 相位误差, 锁定时 → 0)
 *          cosθ/sinθ 使用 **上一拍** 计算结果, 存在一拍延迟
 *
 * #### 第 3 步: PI 环路滤波
 *          ylf[k] = ylf[k-1] + b0 * U_Q[k] + b1 * U_Q[k-1]
 *          U_Q 被 PI 驱动至 0, 即 θ 跟踪电网电压真实相位
 *
 * #### 第 4 步: VCO 频率/相位积分
 *          fo = fn + ylf[k]           (频率 = 额定偏置 + PI 修正量)
 *          θ += fo * Δt * 2π          (相位 = ∫ω dt)
 *          θ 归一化到 [0, 2π)
 *
 * #### 第 5 步: 更新正余弦
 *          sinθ = arm_sin_f32(θ)      (查表或计算, 供下一拍 Park 变换)
 *          cosθ = arm_cos_f32(θ)
 *
 * @note   acValue 应为去直流、归一化后的电压值, 范围建议 ±1.5 左右
 * @note   例: adcRaw 12bit (0~4095), 中点 2048 对应 0V → vNorm = (adcRaw - 2048) / 1365.33
 */
float32_t SPLL_1PH_SOGI_run(SPLL_1PH_SOGI *spll_obj, float32_t acValue);

void SOGI_init(void);

#endif /* __SOGI_H__ */
