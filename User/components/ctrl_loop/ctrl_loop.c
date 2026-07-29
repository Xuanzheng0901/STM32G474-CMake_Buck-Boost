/**
 * @file    ctrl_loop.c
 * @brief   图腾柱 PFC 控制回路
 *
 * ── 电流内环 (20kHz ISR) ──
 *     Iref = I_amplitude × |sin(ωt)|
 *     Duty = D_ideal + PI(|Iref| - |IL|)
 *
 * ── 电压外环 (~100Hz 任务) ──
 *     状态机 → Vref → PI(Vref - Vout) → I_amplitude
 */

#include "ctrl_loop.h"

#include "dac.h"
#include "pfc_config.h"
#include "pfc_utils.h"
#include "pid_ctrl_internal.h"
#include "sogi.h"

#include "FreeRTOS.h"
#include "hrtim.h"
#include "main.h"
#include "queue.h"
#include "task.h"

/* ==================== 外部接口 ==================== */
extern QueueHandle_t ADC_get_dc_queue(void);

/* ==================== 内部状态 ==================== */

static pid_ctrl_block_handle_t i_pid;   /* 电流内环 PI */
static pid_ctrl_block_handle_t v_pid;   /* 电压外环 PID */

static float32_t i_amplitude;           /* 电流参考幅值 */
static float32_t duty_current;          /* 当前占空比 */
static uint8_t hw_polarity;            /* 上次写入硬件的极性 */
static float32_t pol_cos_off = 1.0f;   /* cos(offset) */
static float32_t pol_sin_off = 0.0f;   /* sin(offset) */

static float now_vout_V;                /* DC 输出电压 (V) */
static float now_iout_A;                /* DC 输出电流 (A) */

typedef enum {
    PFC_ZC_HOLD = 0,
    PFC_ZC_BLANK,
    PFC_ZC_PRELOAD,
    PFC_ZC_MAIN_RAMP,
    PFC_ZC_SYNC_DELAY,
    PFC_ZC_RUN
} PfcZcState;

static PfcZcState zc_state;
static uint8_t zc_target_polarity;
static uint16_t zc_blank_cycles;
static uint16_t zc_stable_samples;
static uint16_t zc_zero_current_samples;
static uint16_t zc_ramp_cycles;
static uint16_t zc_sync_delay_cycles;

/* ==================== 辅助宏 ==================== */

/* 生成 PID 配置: 电流内环 */
#define I_PID_CFG(kp_, ki_ts_) { \
    .kp = kp_, .ki = ki_ts_, .kd = 0.0f, \
    .max_output = PFC_I_OUTPUT_MAX,  .min_output = -PFC_I_OUTPUT_MAX, \
    .max_integral = PFC_I_INTEGRAL_MAX, .min_integral = -PFC_I_INTEGRAL_MAX, \
    .cal_type = PID_CAL_TYPE_INCREMENTAL }

/* 生成 PID 配置: 电压外环 */
#define V_PID_CFG(kp_, ki_ts_, kd_) { \
    .kp = kp_, .ki = ki_ts_, .kd = kd_, \
    .max_output = PFC_V_OUTPUT_MAX,  .min_output = 0.0f, \
    .max_integral = PFC_V_INTEGRAL_MAX, .min_integral = 0.0f, \
    .cal_type = PID_CAL_TYPE_INCREMENTAL }

static void ctrl_loop_enter_zc_blank(void)
{
    pfc_power_stage_disable();
    pid_reset_ctrl_block(i_pid);

    duty_current = 0.0f;
    hw_polarity = 0xFFU;
    zc_state = PFC_ZC_BLANK;
    zc_target_polarity = 0xFFU;
    zc_blank_cycles = 0U;
    zc_stable_samples = 0U;
    zc_zero_current_samples = 0U;
    zc_ramp_cycles = 0U;
    zc_sync_delay_cycles = 0U;
}

static void ctrl_loop_hold_power_stage_off(void)
{
    ctrl_loop_enter_zc_blank();
    zc_state = PFC_ZC_HOLD;
}

static uint8_t ctrl_loop_zc_ready(float32_t vin_inst, float32_t il_inst,
                                  uint8_t pll_polarity)
{
    float32_t vin_abs = fabsf(vin_inst);

    if(zc_blank_cycles < PFC_ZC_BLANK_CYCLES)
        zc_blank_cycles++;

    if(fabsf(il_inst) <= PFC_ZC_CURRENT_A)
    {
        if(zc_zero_current_samples < PFC_ZC_ZERO_CURRENT_SAMPLES)
            zc_zero_current_samples++;
    }
    else
    {
        zc_zero_current_samples = 0U;
    }

    if(vin_abs >= PFC_ZC_EXIT_V)
    {
        uint8_t measured_polarity = (vin_inst >= 0.0f) ? 0U : 1U;

        if(measured_polarity == pll_polarity)
        {
            if(zc_target_polarity != measured_polarity)
            {
                zc_target_polarity = measured_polarity;
                zc_stable_samples = 1U;
            }
            else if(zc_stable_samples < PFC_ZC_STABLE_SAMPLES)
            {
                zc_stable_samples++;
            }
        }
        else
        {
            zc_target_polarity = 0xFFU;
            zc_stable_samples = 0U;
        }
    }
    else
    {
        zc_target_polarity = 0xFFU;
        zc_stable_samples = 0U;
    }

    return (zc_blank_cycles >= PFC_ZC_BLANK_CYCLES) &&
           (zc_stable_samples >= PFC_ZC_STABLE_SAMPLES) &&
           (zc_zero_current_samples >= PFC_ZC_ZERO_CURRENT_SAMPLES);
}

static void ctrl_loop_preload_main_ramp(void)
{
    duty_current = PFC_DUTY_MIN;
    zc_ramp_cycles = 0U;
    zc_state = PFC_ZC_PRELOAD;

    pfc_write_duty(duty_current, zc_target_polarity);
}

static void ctrl_loop_main_ramp_step(float32_t vin_inst, float32_t vout,
                                     uint8_t pll_polarity)
{
    uint8_t measured_polarity = (vin_inst >= 0.0f) ? 0U : 1U;

    if((fabsf(vin_inst) <= PFC_ZC_ENTER_V) ||
       (measured_polarity != zc_target_polarity) ||
       (pll_polarity != zc_target_polarity))
    {
        ctrl_loop_enter_zc_blank();
        return;
    }

    if(zc_ramp_cycles < PFC_ZC_RAMP_CYCLES)
        zc_ramp_cycles++;

    float32_t target_duty = pfc_calc_ideal_duty(vin_inst, vout);
    float32_t ramp_ratio =
        (float32_t)zc_ramp_cycles / (float32_t)PFC_ZC_RAMP_CYCLES;
    duty_current = PFC_DUTY_MIN +
                   (target_duty - PFC_DUTY_MIN) * ramp_ratio;
    pfc_write_duty(duty_current, zc_target_polarity);

    if(zc_ramp_cycles >= PFC_ZC_RAMP_CYCLES)
    {
        pfc_set_polarity(zc_target_polarity);
        hw_polarity = zc_target_polarity;
        zc_sync_delay_cycles = 0U;
        zc_state = PFC_ZC_SYNC_DELAY;
    }
}

static void ctrl_loop_sync_delay_step(float32_t vin_inst, uint8_t pll_polarity)
{
    uint8_t measured_polarity = (vin_inst >= 0.0f) ? 0U : 1U;

    if((fabsf(vin_inst) <= PFC_ZC_ENTER_V) ||
       (measured_polarity != zc_target_polarity) ||
       (pll_polarity != zc_target_polarity))
    {
        ctrl_loop_enter_zc_blank();
        return;
    }

    if(zc_sync_delay_cycles < PFC_ZC_SYNC_DELAY_CYCLES)
        zc_sync_delay_cycles++;

    if(zc_sync_delay_cycles >= PFC_ZC_SYNC_DELAY_CYCLES)
    {
        pfc_fast_sync_enable(zc_target_polarity);
        zc_state = PFC_ZC_RUN;
        pid_reset_ctrl_block(i_pid);
    }
}

/* ==================== AC 采样 ISR (20kHz) ==================== */

void ctrl_loop_ac_isr(uint32_t adc_word)
{
    int32_t v_raw = (int32_t)(adc_word & 0x0FFF);
    int32_t i_raw = (int32_t)(adc_word >> 16);

    /* SOGI-PLL */
    SPLL_1PH_SOGI_run(&spll, (float32_t)(v_raw - PFC_VIN_OFFSET) / PFC_SOGI_NORM_DIV);

    /* 滑动平均滤波 */
    static float32_t v_filt, i_filt;
    v_filt += ((float32_t)(v_raw - PFC_VIN_OFFSET) / PFC_VIN_LSB_PER_V - v_filt) * 0.7f;
    i_filt += ((float32_t)(i_raw - PFC_IIN_OFFSET) / PFC_IIN_LSB_PER_A - i_filt) * 0.7f;

    /* 慢桥臂极性 (基于电压过零) */
    uint8_t polarity = (spll.cosine * pol_cos_off >= spll.sine * pol_sin_off) ? 0 : 1;

    /* PF 固定为 1: 电流参考与 PLL 提取的电网电压基波同相。 */
    float32_t ref_wave = spll.cosine;
    float32_t abs_ref = fabsf(ref_wave);
    uint8_t i_sign = (ref_wave >= 0.0f) ? 0 : 1;

    ctrl_loop_current_isr(v_filt, i_filt, now_vout_V, abs_ref, polarity, i_sign);
}

/* ==================== DC 数据处理 (任务上下文) ==================== */

static void dc_data_process(uint32_t *buf)
{
    uint16_t len = ADC_BUFFER_LENGTH / 2;
    uint32_t v_sum = 0, i_sum = 0;

    for(uint16_t j = 0; j < len; j++)
    {
        v_sum += (buf[j] & 0x0FFF);
        i_sum += (buf[j] >> 16);
    }

    now_vout_V = ((float)v_sum / len - PFC_VOUT_OFFSET) / PFC_VOUT_LSB_PER_V;
    now_iout_A = ((float)i_sum / len - PFC_IOUT_OFFSET) / PFC_IOUT_LSB_PER_A;

    /* 状态机更新 (含故障检测) → Vref */
    float32_t vref = ctrl_loop_state_update(now_vout_V, now_iout_A);
    if(ctrl_loop_state_get() == CTRL_STATE_FAULT)
    {
        i_amplitude = 0.0f;
        return;
    }

    /* 电压外环 PI */
    float result;
    pid_compute(v_pid, vref - now_vout_V, &result);
    i_amplitude = result;
}

/* ==================== FreeRTOS 控制任务 ==================== */

static void ctrl_loop_routine(void *pvParameters)
{
    uint32_t *buf;
    while(1)
    {
        if(xQueueReceive(ADC_get_dc_queue(), &buf, portMAX_DELAY) == pdTRUE)
            dc_data_process(buf);
    }
}

/* ==================== 公开 API ==================== */

void ctrl_loop_init(void)
{
    /* 电流内环 PI */
    pid_ctrl_parameter_t i_param = I_PID_CFG(PFC_I_KP_DEFAULT,
                                             PFC_I_KI_DEFAULT / PFC_PWM_FREQ);
    pid_ctrl_config_t i_cfg = {.init_param = i_param};
    pid_new_control_block(&i_cfg, &i_pid);

    /* 电压外环 PID */
    float v_ts = 1.0f / PFC_VOLTAGE_LOOP_FREQ;
    pid_ctrl_parameter_t v_param = V_PID_CFG(PFC_V_KP_DEFAULT,
                                             PFC_V_KI_DEFAULT * v_ts,
                                             PFC_V_KD_DEFAULT);
    pid_ctrl_config_t v_cfg = {.init_param = v_param};
    pid_new_control_block(&v_cfg, &v_pid);

    /* 状态机 */
    ctrl_loop_state_init(PFC_NOMINAL_VOUT,
                         (uint32_t)(PFC_SOFTSTART_SEC * PFC_VOLTAGE_LOOP_FREQ));

    i_amplitude = 0.0f;
    duty_current = 0.0f;
    now_vout_V = 0.0f;
    now_iout_A = 0.0f;

    ctrl_loop_set_polarity_offset(0.0f);

    /* 功率级初始安全态: 快慢桥全关, 体二极管完成预充电。 */
    ctrl_loop_hold_power_stage_off();

    xTaskCreate(ctrl_loop_routine, "CtrlLoop", 2048, NULL, 15, NULL);
}

CtrlLoop_State ctrl_loop_get_state(void)
{
    return ctrl_loop_state_get();
}

/* ---- 电流内环 (20kHz ISR) ---- */

void ctrl_loop_current_isr(float32_t vin_inst, float32_t il_inst,
                           float32_t vout, float32_t abs_ref,
                           uint8_t polarity, uint8_t i_sign)
{
    if((ctrl_loop_state_get() == CTRL_STATE_FAULT) ||
       (vout < PFC_MIN_RUN_VOUT_V))
    {
        if(zc_state != PFC_ZC_HOLD)
            ctrl_loop_hold_power_stage_off();
        return;
    }

    if(zc_state == PFC_ZC_HOLD)
    {
        ctrl_loop_enter_zc_blank();
        return;
    }

    if(zc_state == PFC_ZC_BLANK)
    {
        if(ctrl_loop_zc_ready(vin_inst, il_inst, polarity))
            ctrl_loop_preload_main_ramp();
        return;
    }

    if(zc_state == PFC_ZC_PRELOAD)
    {
        uint8_t measured_polarity = (vin_inst >= 0.0f) ? 0U : 1U;
        if((fabsf(vin_inst) <= PFC_ZC_ENTER_V) ||
           (measured_polarity != zc_target_polarity) ||
           (polarity != zc_target_polarity))
        {
            ctrl_loop_enter_zc_blank();
            return;
        }

        /* CMP 预装载已跨过一次周期更新，此时主开关处于关断区间。 */
        pfc_fast_main_enable(zc_target_polarity);
        zc_state = PFC_ZC_MAIN_RAMP;
        return;
    }

    if(zc_state == PFC_ZC_MAIN_RAMP)
    {
        ctrl_loop_main_ramp_step(vin_inst, vout, polarity);
        return;
    }

    if(zc_state == PFC_ZC_SYNC_DELAY)
    {
        ctrl_loop_sync_delay_step(vin_inst, polarity);
        return;
    }

    float i_fb = (i_sign == 0U) ? il_inst : -il_inst;
    if((fabsf(vin_inst) <= PFC_ZC_ENTER_V) ||
       (polarity != hw_polarity) ||
       (i_fb < -PFC_REVERSE_CURRENT_A))
    {
        ctrl_loop_enter_zc_blank();
        return;
    }

    float i_ref = i_amplitude * abs_ref;
    float i_corr;
    pid_compute(i_pid, i_ref - i_fb, &i_corr);

    /* PF=1 正向 Boost 前馈。 */
    float vin_abs = fabsf(vin_inst);
    float ff = 1.0f - vin_abs / vout;
    duty_current = ff + i_corr;

#if PFC_DEBUG_DAC_OUTPUT
    HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R,
                     (uint16_t)(duty_current * 4096.0f));
#endif
    pfc_write_duty(duty_current, polarity);
}

/* ---- 运行时调谐 ---- */

void ctrl_loop_set_vout(float32_t vout)
{
    ctrl_loop_state_set_vout(vout);
}

void ctrl_loop_set_polarity_offset(float32_t deg)
{
    float32_t rad = deg * 0.0174533f;
    pol_cos_off = arm_cos_f32(rad);
    pol_sin_off = arm_sin_f32(rad);
}

void ctrl_loop_set_current_pi(float32_t kp, float32_t ki)
{
    pid_ctrl_parameter_t p = I_PID_CFG(kp, ki / PFC_PWM_FREQ);
    pid_update_parameters(i_pid, &p);
}

void ctrl_loop_set_voltage_pi(float32_t kp, float32_t ki, float32_t kd)
{
    float v_ts = 1.0f / PFC_VOLTAGE_LOOP_FREQ;
    pid_ctrl_parameter_t p = V_PID_CFG(kp, ki * v_ts, kd);
    pid_update_parameters(v_pid, &p);
}

void ctrl_loop_clear_fault(void)
{
    pfc_power_stage_disable();
    pid_reset_ctrl_block(i_pid);
    pid_reset_ctrl_block(v_pid);
    ctrl_loop_state_clear_fault();
    i_amplitude = 0.0f;
    zc_state = PFC_ZC_HOLD;
    hw_polarity = 0xFFU;
}

/* ---- Getter ---- */

float32_t ctrl_loop_get_vref(void)
{
    return ctrl_loop_state_get_vref();
}

float32_t ctrl_loop_get_voltage(void)
{
    return now_vout_V;
}

float32_t ctrl_loop_get_current(void)
{
    return now_iout_A;
}

float32_t ctrl_loop_get_i_amplitude(void)
{
    return i_amplitude;
}

float32_t ctrl_loop_get_duty(void)
{
    return duty_current;
}
