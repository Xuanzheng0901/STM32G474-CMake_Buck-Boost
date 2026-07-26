/**
 * @file    ctrl_loop.c
 * @brief   图腾柱 PFC 控制回路
 *
 * ── 电流内环 (30kHz ISR) ──
 *     Iref = I_amplitude × |sin(ωt + φ)|
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
static uint8_t  hw_polarity;            /* 上次写入硬件的极性 */
static float32_t pol_cos_off  = 1.0f;   /* cos(offset) */
static float32_t pol_sin_off  = 0.0f;   /* sin(offset) */
static float32_t pf_cos_phi   = 1.0f;   /* cos(φ), PF 相位偏移 (1=unity) */
static float32_t pf_sin_phi   = 0.0f;   /* sin(φ), PF 相位偏移 */

static float now_vout_V;                /* DC 输出电压 (V) */
static float now_iout_A;                /* DC 输出电流 (A) */

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

/* ==================== AC 采样 ISR (30kHz) ==================== */

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

    /* ref_wave = sin(ωt + φ). |ref_wave|→幅值, sign→反馈符号,
     * sign(v_filt)×sign(ref_wave)<0 → 反向功率 → 切换 Buck 前馈 */
    float32_t ref_wave = spll.cosine * pf_cos_phi - spll.sine * pf_sin_phi;
    float32_t abs_ref  = fabsf(ref_wave);
    uint8_t   i_sign   = (ref_wave >= 0.0f) ? 0 : 1;
    uint8_t   reverse  = (v_filt * ref_wave < 0.0f) ? 1 : 0;

    ctrl_loop_current_isr(v_filt, i_filt, now_vout_V, abs_ref, polarity, i_sign, reverse);
}

/* ==================== DC 数据处理 (任务上下文) ==================== */

static void dc_data_process(uint32_t *buf)
{
    uint16_t len = ADC_BUFFER_LENGTH / 2;
    uint32_t v_sum = 0, i_sum = 0;

    for (uint16_t j = 0; j < len; j++) {
        v_sum += (buf[j] & 0x0FFF);
        i_sum += (buf[j] >> 16);
    }

    now_vout_V = ((float)v_sum / len - PFC_VOUT_OFFSET) / PFC_VOUT_LSB_PER_V;
    now_iout_A = ((float)i_sum / len - PFC_IOUT_OFFSET) / PFC_IOUT_LSB_PER_A;

    /* 状态机更新 (含故障检测) → Vref */
    float32_t vref = ctrl_loop_state_update(now_vout_V, now_iout_A);
    if (ctrl_loop_state_get() == CTRL_STATE_FAULT) {
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
    while (1) {
        if (xQueueReceive(ADC_get_dc_queue(), &buf, portMAX_DELAY) == pdTRUE)
            dc_data_process(buf);
    }
}

/* ==================== 公开 API ==================== */

void ctrl_loop_init(void)
{
    /* 电流内环 PI */
    pid_ctrl_parameter_t i_param = I_PID_CFG(PFC_I_KP_DEFAULT,
                                             PFC_I_KI_DEFAULT / PFC_PWM_FREQ);
    pid_ctrl_config_t i_cfg = { .init_param = i_param };
    pid_new_control_block(&i_cfg, &i_pid);

    /* 电压外环 PID */
    float v_ts = 1.0f / PFC_VOLTAGE_LOOP_FREQ;
    pid_ctrl_parameter_t v_param = V_PID_CFG(PFC_V_KP_DEFAULT,
                                             PFC_V_KI_DEFAULT * v_ts,
                                             PFC_V_KD_DEFAULT);
    pid_ctrl_config_t v_cfg = { .init_param = v_param };
    pid_new_control_block(&v_cfg, &v_pid);

    /* 状态机 */
    ctrl_loop_state_init(PFC_NOMINAL_VOUT,
                         (uint32_t)(PFC_SOFTSTART_SEC * PFC_VOLTAGE_LOOP_FREQ));

    i_amplitude = 0.0f;
    duty_current = 0.0f;
    hw_polarity = 0;
    now_vout_V = 0.0f;
    now_iout_A = 0.0f;

    ctrl_loop_set_polarity_offset(-1.0f);

    /* 慢桥臂初始安全态: 双管关闭, 体二极管充当整流 */
    pfc_set_polarity(0);

    xTaskCreate(ctrl_loop_routine, "CtrlLoop", 2048, NULL, 15, NULL);
}

CtrlLoop_State ctrl_loop_get_state(void)
{
    return ctrl_loop_state_get();
}

/* ---- 电流内环 (30kHz ISR) ---- */

void ctrl_loop_current_isr(float32_t vin_inst, float32_t il_inst,
                           float32_t vout, float32_t abs_ref,
                           uint8_t polarity, uint8_t i_sign, uint8_t reverse)
{
    /* 慢桥臂: 基于电压过零换向 */
    if (polarity != hw_polarity) {
        pfc_set_polarity(polarity);
        hw_polarity = polarity;
    }

    if (vout < 3.0f) {
        duty_current = 0.0f;
    } else {
        float i_ref = i_amplitude * abs_ref;
        float i_fb  = (i_sign == 0) ? il_inst : -il_inst;
        float i_corr;
        pid_compute(i_pid, i_ref - i_fb, &i_corr);

        /* 前馈: 正向功率→Boost, 反向功率→Buck (同步整流主导) */
        float vin_abs = fabsf(vin_inst);
        float ff = reverse
            ? vin_abs / vout                    /* Buck: DC→AC */
            : 1.0f - vin_abs / vout;            /* Boost: AC→DC */
        duty_current = ff + i_corr;
    }

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
    ctrl_loop_state_clear_fault();
    i_amplitude = 0.0f;
}

/* ---- PF 设定 ---- */

void ctrl_loop_set_pf(float32_t pf)
{
    /* pf = cos(φ): -1~1, 正=容性(超前), 负=感性(滞后)
     * cos_phi = |pf|,  sin_phi = sign(pf) × √(1-pf²) */
    float32_t abs_pf = fabsf(pf);
    if (abs_pf > 1.0f) abs_pf = 1.0f;
    pf_cos_phi = abs_pf;
    arm_sqrt_f32(1.0f - abs_pf * abs_pf, &pf_sin_phi);
    if (pf < 0.0f) pf_sin_phi = -pf_sin_phi;
}

float32_t ctrl_loop_get_pf(void)
{
    return (pf_sin_phi >= 0.0f) ? pf_cos_phi : -pf_cos_phi;
}

/* ---- Getter ---- */

float32_t ctrl_loop_get_vref(void)       { return ctrl_loop_state_get_vref(); }
float32_t ctrl_loop_get_voltage(void)    { return now_vout_V; }
float32_t ctrl_loop_get_current(void)    { return now_iout_A; }
float32_t ctrl_loop_get_i_amplitude(void){ return i_amplitude; }
float32_t ctrl_loop_get_duty(void)       { return duty_current; }
