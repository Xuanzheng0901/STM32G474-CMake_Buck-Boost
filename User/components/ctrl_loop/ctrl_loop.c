/**
 * @file    ctrl_loop.c
 * @brief   图腾柱 PFC 总控制回路实现
 *
 * @details
 * ── 电流内环 (30kHz) ──
 *   由 ADC ISR 调用 ctrl_loop_current_isr():
 *     Iref = I_amplitude × |sinθ|
 *     Duty = D_ideal + PI(Iref - IL)
 *     写入 HRTIM Timer A CMP 寄存器
 *
 * ── 电压外环 (~300Hz) ──
 *   由 FreeRTOS 任务调用 ctrl_loop_voltage_task():
 *     软启动管理 Vref 斜坡
 *     I_amplitude = PI(Vref - Vout)
 */

#include "ctrl_loop.h"
#include "pfc_utils.h"
#include "pfc_config.h"
#include "hrtim.h"

/* ==================== 内部状态 ==================== */

static PI_Controller  i_pi;            /**< 电流内环 PI */
static PI_Controller  v_pi;            /**< 电压外环 PI */

static CtrlLoop_State state;           /**< 当前控制状态 */
static float32_t i_amplitude;          /**< 电流参考幅值 (电压环输出) */
static float32_t duty_current;         /**< 当前占空比 (用于调试) */
static float32_t vref_ramp;            /**< 软启动斜坡电压 */
static uint32_t  soft_start_ticks;     /**< 软启动已进行节拍数 */
static uint8_t   prev_polarity;        /**< 上一拍极性 (减少重复 HRTIM 写入) */

/* ==================== 实现 ==================== */

void ctrl_loop_init(void)
{
    /* 电流内环 PI: Ki 预乘 Ts(1/30kHz) */
    float32_t i_ki = PFC_I_KI_DEFAULT * (1.0f / PFC_PWM_FREQ);

    pi_init(&i_pi,
            PFC_I_KP_DEFAULT, i_ki,
            PFC_I_INTEGRAL_MAX, -PFC_I_INTEGRAL_MAX,
            PFC_I_OUTPUT_MAX,  -PFC_I_OUTPUT_MAX);

    /* 电压外环 PI: Ki 预乘 Ts(电压环周期 = VOLTAGE_LOOP_DIV / 30kHz) */
    float32_t v_ts = (float32_t)PFC_VOLTAGE_LOOP_DIV / PFC_PWM_FREQ;
    float32_t v_ki = PFC_V_KI_DEFAULT * v_ts;

    pi_init(&v_pi,
            PFC_V_KP_DEFAULT, v_ki,
            PFC_V_INTEGRAL_MAX, 0.0f,               /* 积分 ≥ 0, 不输出负电流 */
            PFC_V_OUTPUT_MAX,   0.0f);

    /* 软启动: Vref 从 0 逐步上升到目标值 */
    state             = CTRL_STATE_SOFT_START;
    vref_ramp         = 0.0f;
    soft_start_ticks  = 0;
    i_amplitude       = 0.0f;
    duty_current      = 0.0f;
    prev_polarity     = 0;

    /* Timer B 安全态: 全关 */
    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP1xR = 0;
    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP3xR = 0;
}

CtrlLoop_State ctrl_loop_get_state(void)
{
    return state;
}

/* ---- 电流内环 (30kHz ISR) ---- */

void ctrl_loop_current_isr(float32_t vin_inst, float32_t il_inst,
                           float32_t vout,
                           float32_t abs_sin, uint8_t polarity)
{
    /* 1. 工频桥臂换向 (仅在极性翻转时写入, 减少 HRTIM 访问) */
    if (polarity != prev_polarity) {
        pfc_set_polarity(polarity);
        prev_polarity = polarity;
    }

    /* 2. 瞬时电流参考: Iref = I_amplitude × |sinθ| */
    float32_t i_ref = pfc_gen_i_ref(i_amplitude, abs_sin);

    /* 3. 电流环 PI: 误差 → 占空比修正量 */
    float32_t i_error      = i_ref - il_inst;
    float32_t i_correction = pi_compute(&i_pi, i_error);

    /* 4. 占空比 = 理想 Boost 前馈 + PI 修正 */
    float32_t duty_ideal = pfc_calc_ideal_duty(vin_inst, vout);
    duty_current = duty_ideal + i_correction;

    /* 5. 写入 HRTIM Timer A (下个 PWM 周期生效) */
    pfc_write_duty(duty_current);
}

/* ---- 电压外环 (~300Hz, FreeRTOS 任务) ---- */

void ctrl_loop_voltage_task(float32_t vout_measured)
{
    if (state == CTRL_STATE_IDLE || state == CTRL_STATE_FAULT) {
        i_amplitude = 0.0f;
        return;
    }

    /* 软启动: Vref 从 0 逐步上升到目标值, 防止浪涌 */
    if (state == CTRL_STATE_SOFT_START) {
        soft_start_ticks++;
        vref_ramp = PFC_NOMINAL_VOUT *
                    ((float32_t)soft_start_ticks / (float32_t)PFC_SOFTSTART_STEPS);

        if (soft_start_ticks >= PFC_SOFTSTART_STEPS) {
            vref_ramp = PFC_NOMINAL_VOUT;
            state = CTRL_STATE_RUNNING;
        }
    }

    /* 电压环 PI: Vref - Vout → 电流参考幅值 */
    float32_t v_error = vref_ramp - vout_measured;
    i_amplitude = pi_compute(&v_pi, v_error);
}

/* ---- Getter ---- */

float32_t ctrl_loop_get_i_amplitude(void) { return i_amplitude; }
float32_t ctrl_loop_get_duty(void)        { return duty_current; }
float32_t ctrl_loop_get_vref(void)        { return vref_ramp; }
