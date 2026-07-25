/**
 * @file    ctrl_loop.c
 * @brief   图腾柱 PFC 总控制回路: 电流内环 + 电压外环 + ADC 数据处理 + 软启动
 *
 * @details
 * ── FreeRTOS 任务 (ctrl_loop_routine) ──
 *   等待 ADC DMA 队列 → RMS + 卡尔曼滤波 → 电压外环 PI (~300Hz)
 *   → 更新 ISR 使用的 Vout 缓存
 *
 * ── 电流内环 (ADC ISR 调用, 30kHz) ──
 *   Iref = I_amplitude × |sinθ|
 *   Duty = D_ideal + PI(Iref - IL)
 *   写入 HRTIM Timer A CMP 寄存器
 */

#include "ctrl_loop.h"

#include "dac.h"
#include "pfc_utils.h"
#include "pfc_config.h"
#include "pid_ctrl_internal.h"
#include "sogi.h"

#include "FreeRTOS.h"
#include "hrtim.h"
#include "main.h"
#include "queue.h"
#include "task.h"

/* ==================== 外部引用 ==================== */

extern SPLL_1PH_SOGI spll;
extern QueueHandle_t dc_adc_queue;

/* ==================== 内部状态 ==================== */

static pid_ctrl_block_handle_t i_pid = NULL;  /**< 电流内环 PID (Kd=0 → PI) */
static pid_ctrl_block_handle_t v_pid = NULL;  /**< 电压外环 PID (Kd=0 → PI) */

static CtrlLoop_State state;           /**< 当前控制状态 */
static float32_t i_amplitude;          /**< 电流参考幅值 (电压环输出) */
static float32_t duty_current;         /**< 当前占空比 (用于调试) */
static float32_t vref_ramp;            /**< 电压环参考斜坡 (运行时变量) */
static float32_t vout_target;          /**< 目标输出电压 (可通过 API 运行时修改) */
static uint32_t soft_start_ticks;     /**< 软启动已进行节拍数 */
static uint32_t soft_start_total;     /**< 软启动总节拍数 (频率 × 秒数) */
static uint8_t hw_polarity;         /**< 上次写入硬件的极性 */
static float32_t pol_cos_off;         /**< cos(offset) 慢管相位补偿 */
static float32_t pol_sin_off;         /**< sin(offset) 慢管相位补偿 */

/* 直流测量值 (由 dc_data_process 更新, 供 UI 读取和 ISR 使用) */
static float now_vout_V = 0.0f;       /**< DC 输出电压 (V) */
static float now_iout_A = 0.0f;       /**< DC 输出电流 (A) */

/* ==================== 前向声明 ==================== */

static void dc_data_process(uint32_t *data_buf);

static void ctrl_loop_routine(void *pvParameters);

//
// /* ==================== HRTIM 回调 (调试用) ==================== */
//
// void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t TimerIdx)
// {
//     if(TimerIdx != HRTIM_TIMERINDEX_MASTER)
//         return;
//
//     GPIOC->ODR ^= GPIO_PIN_1;
//     GPIOC->ODR ^= GPIO_PIN_1;
// }

/* ==================== AC 采样处理 (30kHz ISR 上下文) ==================== */

void ctrl_loop_ac_isr(uint32_t adc_word)
{
    /* 提取 ADC1 电压 (低 12bit) 和 ADC2 电流 (高 16bit) */
    int32_t v_raw = (int32_t)(adc_word & 0x0FFF);
    int32_t i_raw = (int32_t)(adc_word >> 16);

    /* SOGI-PLL: 归一化到 ~±1 */
    float32_t v_sogi = (float32_t)(v_raw - 2048) / 1365.33f;
    SPLL_1PH_SOGI_run(&spll, v_sogi);

    /* 实际物理量 + 滑动平均抗噪 */
    static float32_t v_filt = 0.0f, i_filt = 0.0f;
    float32_t v_inst = (float32_t)(v_raw - PFC_VIN_OFFSET) / PFC_VIN_LSB_PER_V;
    float32_t i_inst = (float32_t)(i_raw - PFC_IIN_OFFSET) / PFC_IIN_LSB_PER_A;
    v_filt = v_filt * 0.3f + v_inst * 0.7f;
    i_filt = i_filt * 0.3f + i_inst * 0.7f;

    /* 极性 + |cos| */
    float32_t cos_shifted = spll.cosine * pol_cos_off - spll.sine * pol_sin_off;
    uint8_t polarity = (cos_shifted >= 0.0f) ? 0 : 1;
    float32_t abs_cos = (spll.cosine >= 0.0f) ? spll.cosine : -spll.cosine;

    /* 电流内环 */
    ctrl_loop_current_isr(v_filt, i_filt, now_vout_V,
                          abs_cos, polarity);
}

/* ==================== DC 数据处理 (任务上下文, ~150Hz) ==================== */

static void dc_data_process(uint32_t *data_buf)
{
    uint16_t len = ADC_BUFFER_LENGTH / 2;
    uint32_t v_sum = 0, i_sum = 0;

    for(uint16_t j = 0; j < len; j++)
    {
        v_sum += (data_buf[j] & 0x0FFF);        /* ADC3: DC 电压 */
        i_sum += (data_buf[j] >> 16);            /* ADC4: DC 电流 */
    }

    float v_avg = (float)v_sum / (float)len;
    float i_avg = (float)i_sum / (float)len;

    /* 实际物理量 (V, A) */
    now_vout_V = (v_avg - PFC_VOUT_OFFSET) / PFC_VOUT_LSB_PER_V;
    now_iout_A = (i_avg - PFC_IOUT_OFFSET) / PFC_IOUT_LSB_PER_A;

    /* 电压外环: Vref - Vout → 电流参考幅值 */
    ctrl_loop_voltage_task(now_vout_V);
}

/* ==================== FreeRTOS 控制任务 ==================== */

static void ctrl_loop_routine(void *pvParameters)
{
    static uint32_t *buf_ptr;

    while(1)
    {
        /* 等待 DC 数据 (ADC34, ~150Hz) */
        if(xQueueReceive(dc_adc_queue, &buf_ptr, portMAX_DELAY) == pdTRUE)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_1);

            dc_data_process(buf_ptr);   /* Vout 平均 + 电压外环 */

            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_1);
        }
    }
}

/* ==================== 公开 API ==================== */

void ctrl_loop_init(void)
{
    /* ---- 电流内环 PID (Kd=0, 增量式) ---- */
    float i_ki = PFC_I_KI_DEFAULT / PFC_PWM_FREQ;  /* 连续 Ki → 离散: Ki*Ts */

    pid_ctrl_config_t i_cfg = {
        .init_param = {
            .kp           = PFC_I_KP_DEFAULT,
            .ki           = i_ki,
            .kd           = 0.0f,
            .max_output   = PFC_I_OUTPUT_MAX,
            .min_output   = -PFC_I_OUTPUT_MAX,
            .max_integral = PFC_I_INTEGRAL_MAX,
            .min_integral = -PFC_I_INTEGRAL_MAX,
            .cal_type     = PID_CAL_TYPE_INCREMENTAL,
        }
    };
    pid_new_control_block(&i_cfg, &i_pid);

    /* ---- 电压外环 PID (Kd=0, 增量式) ---- */
    float v_ts = 1.0f / PFC_VOLTAGE_LOOP_FREQ;  /* 100Hz → 10ms */
    float v_ki = PFC_V_KI_DEFAULT * v_ts;

    pid_ctrl_config_t v_cfg = {
        .init_param = {
            .kp           = PFC_V_KP_DEFAULT,
            .ki           = v_ki,
            .kd           = PFC_V_KD_DEFAULT,
            .max_output   = PFC_V_OUTPUT_MAX,
            .min_output   = 0.0f,
            .max_integral = PFC_V_INTEGRAL_MAX,
            .min_integral = 0.0f,
            .cal_type     = PID_CAL_TYPE_INCREMENTAL,
        }
    };
    pid_new_control_block(&v_cfg, &v_pid);

    /* 软启动 */
    state = CTRL_STATE_SOFT_START;
    vout_target = PFC_NOMINAL_VOUT;
    vref_ramp = 0.0f;
    soft_start_ticks = 0;
    soft_start_total = (uint32_t)(PFC_SOFTSTART_SEC * PFC_VOLTAGE_LOOP_FREQ);
    i_amplitude = 0.0f;
    duty_current = 0.0f;
    hw_polarity = 0;
    pol_cos_off = 1.0f;   /* cos(0°) */
    pol_sin_off = 0.0f;   /* sin(0°) */
    now_vout_V = 0.0f;
    now_iout_A = 0.0f;

    ctrl_loop_set_polarity_offset(-1.0f);

    /* 创建控制任务 */
    xTaskCreate(ctrl_loop_routine, "CtrlLoop", 2048, NULL, 15, NULL);
}

CtrlLoop_State ctrl_loop_get_state(void)
{
    return state;
}

/* ---- 电流内环 (30kHz ISR) ---- */

void ctrl_loop_current_isr(float32_t vin_inst, float32_t il_inst,
                           float32_t vout,
                           float32_t abs_cos, uint8_t polarity)
{
    if(polarity != hw_polarity)
    {
        pfc_set_polarity(polarity);
        hw_polarity = polarity;
    }

    /* 启动保护: Vout < 3V 时等体二极管预充电 */
    if(vout < 3.0f)
    {
        duty_current = 0.0f;
    }
    else
    {
        float i_ref = pfc_gen_i_ref(i_amplitude, abs_cos);
        float i_fb = (polarity == 0) ? il_inst : -il_inst;
        float i_error = i_ref - i_fb;
        float i_correction;
        pid_compute(i_pid, i_error, &i_correction);

        float32_t duty_ideal = pfc_calc_ideal_duty(vin_inst, vout);
        duty_current = duty_ideal + i_correction;
    }

    HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (uint16_t)(duty_current * 4096.0f));

    pfc_write_duty(duty_current, polarity);
}

/* ---- 电压外环 (~300Hz) ---- */

void ctrl_loop_voltage_task(float32_t vout_measured)
{
    if(state == CTRL_STATE_IDLE || state == CTRL_STATE_FAULT)
    {
        i_amplitude = 0.0f;
        return;
    }

    if(state == CTRL_STATE_SOFT_START)
    {
        soft_start_ticks++;
        vref_ramp = vout_target *
                    ((float32_t)soft_start_ticks / (float32_t)soft_start_total);

        if(soft_start_ticks >= soft_start_total)
        {
            vref_ramp = vout_target;
            state = CTRL_STATE_RUNNING;
        }
    }

    float v_error = vref_ramp - vout_measured;
    float result;
    pid_compute(v_pid, v_error, &result);
    i_amplitude = result;
}

/* ---- 运行时修改目标电压 ---- */

void ctrl_loop_set_vout(float32_t vout)
{
    if(vout < 1.0f)
        vout = 1.0f;
    vout_target = vout;

    if(state == CTRL_STATE_RUNNING)
    {
        state = CTRL_STATE_SOFT_START;
        soft_start_ticks = (uint32_t)(vref_ramp / vout_target * (float)soft_start_total);
    }
}

/* ---- Getter (调试/显示) ---- */

float32_t ctrl_loop_get_i_amplitude(void)
{
    return i_amplitude;
}

float32_t ctrl_loop_get_duty(void)
{
    return duty_current;
}

float32_t ctrl_loop_get_vref(void)
{
    return vref_ramp;
}

float32_t ctrl_loop_get_voltage(void)
{
    return now_vout_V;
}

float32_t ctrl_loop_get_current(void)
{
    return now_iout_A;
}

float32_t ctrl_loop_get_vout_cached(void)
{
    return now_vout_V;
}

void ctrl_loop_set_vout_cache(float32_t vout)
{
    now_vout_V = vout;
}

void ctrl_loop_set_polarity_offset(float32_t deg)
{
    float32_t rad = deg * 0.0174533f;
    pol_cos_off = arm_cos_f32(rad);
    pol_sin_off = arm_sin_f32(rad);
}

void ctrl_loop_set_current_pi(float32_t kp, float32_t ki)
{
    pid_ctrl_parameter_t param = {
        .kp           = kp, .ki                           = ki / PFC_PWM_FREQ, .kd = 0.0f,
        .max_output   = PFC_I_OUTPUT_MAX, .min_output     = -PFC_I_OUTPUT_MAX,
        .max_integral = PFC_I_INTEGRAL_MAX, .min_integral = -PFC_I_INTEGRAL_MAX,
        .cal_type     = PID_CAL_TYPE_INCREMENTAL,
    };
    pid_update_parameters(i_pid, &param);
}

void ctrl_loop_set_voltage_pi(float32_t kp, float32_t ki)
{
    float v_ts = 1.0f / PFC_VOLTAGE_LOOP_FREQ;
    pid_ctrl_parameter_t param = {
        .kp           = kp, .ki                           = ki * v_ts, .kd = 0.0f,
        .max_output   = PFC_V_OUTPUT_MAX, .min_output     = 0.0f,
        .max_integral = PFC_V_INTEGRAL_MAX, .min_integral = 0.0f,
        .cal_type     = PID_CAL_TYPE_INCREMENTAL,
    };
    pid_update_parameters(v_pid, &param);
}
