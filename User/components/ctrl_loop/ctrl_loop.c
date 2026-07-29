/**
 * @file    ctrl_loop.c
 * @brief   图腾柱 PFC 控制回路
 *
 * ── 电流内环 (20kHz ISR) ──
 *     Iref = I_amplitude × sin(ωt)
 *     Duty = D_ideal + sign(Vin) × PR(Iref - IL)
 *
 * ── 电压外环 (~100Hz 任务) ──
 *     状态机 → Vref → PI(Vref - Vout) → I_amplitude
 */

#include "ctrl_loop.h"

#include "dac.h"
#include "pfc_config.h"
#include "pfc_utils.h"
#include "pid_ctrl_internal.h"
#include "pr_ctrl.h"
#include "sogi.h"

#include "FreeRTOS.h"
#include "hrtim.h"
#include "main.h"
#include "queue.h"
#include "task.h"

/* ==================== 外部接口 ==================== */
extern QueueHandle_t ADC_get_dc_queue(void);

/* ==================== 内部状态 ==================== */

static pr_ctrl_t i_pr;                  /* 电流内环准 PR */
static pid_ctrl_block_handle_t v_pid;   /* 电压外环 PID */

static float32_t i_amplitude;           /* 电流参考幅值 */
static float32_t duty_current;          /* 当前占空比 */
static uint8_t hw_polarity;            /* 上次写入硬件的极性 */
static float32_t pol_cos_off = 1.0f;   /* cos(offset) */
static float32_t pol_sin_off = 0.0f;   /* sin(offset) */

static float now_vout_V;                /* DC 输出电压 (V) */
static float now_iout_A;                /* DC 输出电流 (A) */

typedef enum {
    PFC_COMM_HOLD = 0,
    PFC_COMM_PRELOAD,
    PFC_COMM_SLOW_BLANK,
    PFC_COMM_RUN
} PfcCommutationState;

static PfcCommutationState comm_state;
static uint8_t comm_target_polarity;
static uint16_t comm_blank_cycles;
static uint16_t comm_stable_samples;

/* ==================== 辅助宏 ==================== */

/* 生成 PID 配置: 电压外环 */
#define V_PID_CFG(kp_, ki_ts_, kd_) { \
    .kp = kp_, .ki = ki_ts_, .kd = kd_, \
    .max_output = PFC_V_OUTPUT_MAX,  .min_output = 0.0f, \
    .max_integral = PFC_V_INTEGRAL_MAX, .min_integral = 0.0f, \
    .cal_type = PID_CAL_TYPE_INCREMENTAL }

static void ctrl_loop_reset_comm_validation(void)
{
    comm_target_polarity = 0xFFU;
    comm_blank_cycles = 0U;
    comm_stable_samples = 0U;
}

static void ctrl_loop_hold_power_stage_off(void)
{
    pfc_power_stage_disable();
    pr_ctrl_reset(&i_pr);

    duty_current = 0.0f;
    hw_polarity = 0xFFU;
    comm_state = PFC_COMM_HOLD;
    ctrl_loop_reset_comm_validation();
}

static void ctrl_loop_enter_slow_blank(void)
{
    /*
     * 过零时只关闭慢桥。慢管体二极管负责阻断直流母线倒灌，
     * 快桥和电流环继续运行，避免人为制造电流为零的平台。
     */
    pfc_slow_bridge_disable();
    hw_polarity = 0xFFU;
    comm_state = PFC_COMM_SLOW_BLANK;
    ctrl_loop_reset_comm_validation();
}

static uint8_t ctrl_loop_slow_polarity_ready(float32_t vin_inst,
                                             uint8_t pll_polarity)
{

    if(comm_blank_cycles < PFC_ZC_BLANK_CYCLES)
        comm_blank_cycles++;

    if(fabsf(vin_inst) >= PFC_ZC_EXIT_V)
    {
        uint8_t measured_polarity = (vin_inst >= 0.0f) ? 0U : 1U;

        if(measured_polarity == pll_polarity)
        {
            if(comm_target_polarity != measured_polarity)
            {
                comm_target_polarity = measured_polarity;
                comm_stable_samples = 1U;
            }
            else if(comm_stable_samples < PFC_ZC_STABLE_SAMPLES)
            {
                comm_stable_samples++;
            }
        }
        else
        {
            comm_target_polarity = 0xFFU;
            comm_stable_samples = 0U;
        }
    }
    else
    {
        comm_target_polarity = 0xFFU;
        comm_stable_samples = 0U;
    }

    return (comm_blank_cycles >= PFC_ZC_BLANK_CYCLES) &&
           (comm_stable_samples >= PFC_ZC_STABLE_SAMPLES);
}

/* ==================== AC 采样 ISR (20kHz) ==================== */

void ctrl_loop_ac_isr(uint32_t adc_word)
{
    int32_t v_raw = (int32_t)(adc_word & 0x0FFF);
    int32_t i_raw = (int32_t)(adc_word >> 16);

    /*
     * 保存本拍输入对应的 PLL 波形。SOGI_run() 返回前会把相角推进到下一拍，
     * 因此不能在调用后直接使用更新后的 cosine 生成当前采样的参考。
     */
    float32_t ref_wave = spll.cosine;

    /* SOGI-PLL */
    SPLL_1PH_SOGI_run(&spll, (float32_t)(v_raw - PFC_VIN_OFFSET) / PFC_SOGI_NORM_DIV);

    /* 滑动平均滤波 */
    static float32_t v_filt, i_filt;
    v_filt += ((float32_t)(v_raw - PFC_VIN_OFFSET) / PFC_VIN_LSB_PER_V - v_filt) * 0.7f;
    i_filt += ((float32_t)(i_raw - PFC_IIN_OFFSET) / PFC_IIN_LSB_PER_A - i_filt) * 0.7f;

    /* 慢桥臂极性 (基于电压过零) */
    uint8_t polarity = (spll.cosine * pol_cos_off >= spll.sine * pol_sin_off) ? 0 : 1;

    /* PF 固定为 1: 有符号电流参考与当前电网电压采样同相。 */
    ctrl_loop_current_isr(v_filt, i_filt, now_vout_V, ref_wave, polarity);
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
    /* 电流内环准 PR */
    if(!pr_ctrl_configure(&i_pr,
                          PFC_I_PR_KP_DEFAULT,
                          PFC_I_PR_KR_DEFAULT,
                          PFC_I_PR_FREQ_HZ,
                          PFC_I_PR_BANDWIDTH_HZ,
                          PFC_PWM_FREQ,
                          PFC_I_OUTPUT_MAX))
    {
        Error_Handler();
    }

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
                           float32_t vout, float32_t ref_wave,
                           uint8_t polarity)
{
    if((ctrl_loop_state_get() == CTRL_STATE_FAULT) ||
       (vout < PFC_MIN_RUN_VOUT_V))
    {
        if(comm_state != PFC_COMM_HOLD)
            ctrl_loop_hold_power_stage_off();
        return;
    }

    if(comm_state == PFC_COMM_HOLD)
    {
        /*
         * 先写入安全占空比，等待一次 PWM 更新后再打开快桥，
         * 避免使能瞬间使用未初始化的比较值。
         */
        duty_current = pfc_calc_ideal_duty(vin_inst, vout);
        pfc_write_duty(duty_current, polarity);
        comm_state = PFC_COMM_PRELOAD;
        return;
    }

    if(comm_state == PFC_COMM_PRELOAD)
    {
        pfc_fast_bridge_enable();
        ctrl_loop_enter_slow_blank();
        return;
    }

    if(comm_state == PFC_COMM_SLOW_BLANK)
    {
        if(ctrl_loop_slow_polarity_ready(vin_inst, polarity))
        {
            pfc_set_polarity(comm_target_polarity);
            hw_polarity = comm_target_polarity;
            comm_state = PFC_COMM_RUN;
        }
    }
    else if((fabsf(vin_inst) <= PFC_ZC_ENTER_V) ||
            (polarity != hw_polarity))
    {
        ctrl_loop_enter_slow_blank();
    }

    float i_ref = i_amplitude * ref_wave;
    float i_corr_signed = pr_ctrl_compute(&i_pr, i_ref - il_inst);

    /*
     * PR 在静止坐标系输出有符号校正量。负半周增大电流绝对值仍需增大
     * Boost 占空比，因此写入功率级前按半周符号折算为幅值校正。
     */
    float half_cycle_sign = (ref_wave >= 0.0f) ? 1.0f : -1.0f;
    float vin_abs = fabsf(vin_inst);
    float ff = 1.0f - vin_abs / vout;
    duty_current = ff + half_cycle_sign * i_corr_signed;
    if(duty_current > PFC_DUTY_MAX)
        duty_current = PFC_DUTY_MAX;
    else if(duty_current < PFC_DUTY_MIN)
        duty_current = PFC_DUTY_MIN;

#if PFC_DEBUG_DAC_OUTPUT
    HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R,
                     (uint16_t)(duty_current * 4095.0f));
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

bool ctrl_loop_set_current_pr(float32_t kp, float32_t kr, float32_t bandwidth_hz)
{
    pr_ctrl_t updated;
    if(!pr_ctrl_configure(&updated,
                          kp,
                          kr,
                          PFC_I_PR_FREQ_HZ,
                          bandwidth_hz,
                          PFC_PWM_FREQ,
                          PFC_I_OUTPUT_MAX))
    {
        return false;
    }

    /*
     * 参数来自任务上下文，而控制器状态在 ADC ISR 中更新。
     * 完整结构体替换期间短暂关中断，避免 ISR 读到一半新、一半旧的系数。
     */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    i_pr = updated;
    __set_PRIMASK(primask);
    return true;
}

void ctrl_loop_set_voltage_pi(float32_t kp, float32_t ki, float32_t kd)
{
    float v_ts = 1.0f / PFC_VOLTAGE_LOOP_FREQ;
    pid_ctrl_parameter_t p = V_PID_CFG(kp, ki * v_ts, kd);
    pid_update_parameters(v_pid, &p);
}

void ctrl_loop_clear_fault(void)
{
    ctrl_loop_hold_power_stage_off();
    pid_reset_ctrl_block(v_pid);
    ctrl_loop_state_clear_fault();
    i_amplitude = 0.0f;
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
