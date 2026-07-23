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
#include "pfc_utils.h"
#include "pfc_config.h"

#include "FreeRTOS.h"
#include "hrtim.h"
#include "kalman.h"
#include "main.h"
#include "queue.h"
#include "task.h"

#include <math.h>

/* ==================== 外部引用 ==================== */

extern QueueHandle_t adc_queue;

/* ==================== 内部状态 ==================== */

static PI_Controller  i_pi;            /**< 电流内环 PI */
static PI_Controller  v_pi;            /**< 电压外环 PI */

static CtrlLoop_State state;           /**< 当前控制状态 */
static float32_t i_amplitude;          /**< 电流参考幅值 (电压环输出) */
static float32_t duty_current;         /**< 当前占空比 (用于调试) */
static float32_t vref_ramp;            /**< 电压环参考斜坡 (运行时变量) */
static float32_t vout_target;          /**< 目标输出电压 (可通过 API 运行时修改) */
static uint32_t  soft_start_ticks;     /**< 软启动已进行节拍数 */
static uint8_t   prev_polarity;        /**< 上一拍极性 (减少重复 HRTIM 写入) */

/* 测量值 (由 adc_data_process 更新, 供 UI 读取) */
static float now_voltage_mV = 0.0f;    /**< 输出电压 (mV) */
static float now_current_A  = 0.0f;    /**< 输出电流 (A) */

/* ISR 使用的 Vout 缓存 */
static float32_t vout_cached = 0.0f;

/* ==================== 前向声明 ==================== */

static void adc_data_process(uint32_t *data_buf);
static void ctrl_loop_routine(void *pvParameters);

/* ==================== HRTIM 回调 (调试用) ==================== */

void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t TimerIdx)
{
    if(TimerIdx != HRTIM_TIMERINDEX_MASTER)
        return;

    GPIOC->ODR ^= GPIO_PIN_1;
    GPIOC->ODR ^= GPIO_PIN_1;
}

/* ==================== ADC 数据处理 (RMS + 卡尔曼) ==================== */

/** @brief 电压 RMS 系数: ADC 方差 → mV */
#define V_RMS_COEF (28.5f)
/** @brief 电流 RMS 系数: ADC 方差 → A */
#define I_RMS_COEF (0.007326007f)

static void adc_data_process(uint32_t *data_buf)
{
    static kalman_1d_state_t kf_voltage;
    static kalman_1d_state_t kf_current;
    static uint8_t is_kf_initialized = 0;

    uint32_t v_sum = 0, i_sum = 0;
    uint64_t v_sq_sum = 0, i_sq_sum = 0;
    uint16_t len = ADC_BUFFER_LENGTH / 2;

    for(uint16_t i = 0; i < len; i++)
    {
        uint32_t v_raw = data_buf[i] & 0x0FFF;
        uint32_t i_raw = data_buf[i] >> 16;

        v_sum += v_raw;
        i_sum += i_raw;
        v_sq_sum += v_raw * v_raw;
        i_sq_sum += i_raw * i_raw;
    }

    float f_len = (float)len;

    float v_var = ((float)v_sq_sum - ((float)v_sum * v_sum) / f_len) / f_len;
    float i_var = ((float)i_sq_sum - ((float)i_sum * i_sum) / f_len) / f_len;

    if(v_var < 0.0f) v_var = 0.0f;
    if(i_var < 0.0f) i_var = 0.0f;

    float raw_voltage_mV = sqrtf(v_var) * V_RMS_COEF;
    float raw_current_A  = sqrtf(i_var) * I_RMS_COEF;

    if(!is_kf_initialized)
    {
        kalman_1d_init(&kf_voltage, raw_voltage_mV, 10.0f, 0.5f, 50.0f);
        kalman_1d_init(&kf_current, raw_current_A,   1.0f, 0.01f, 1.0f);
        is_kf_initialized = 1;
    }

    now_voltage_mV = kalman_1d_update(&kf_voltage, raw_voltage_mV);
    now_current_A  = kalman_1d_update(&kf_current,  raw_current_A);
}

/* ==================== FreeRTOS 控制任务 ==================== */

static void ctrl_loop_routine(void *pvParameters)
{
    static uint32_t target_voltage_buffer_mV = 0;
    static uint32_t *buf_ptr;

    /* 电压外环分频计数器 (~300Hz = 每 5 次 ADC 数据处理) */
    static uint32_t vloop_div = 0;

    while(1)
    {
        if(xQueueReceive(adc_queue, &buf_ptr, portMAX_DELAY) == pdTRUE)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_1);

            adc_data_process(buf_ptr);

            /* ---- PFC 电压外环 (~300Hz) ---- */
            vloop_div++;
            if(vloop_div >= 5)
            {
                vloop_div = 0;
                float32_t vout_volts = now_voltage_mV / 1000.0f;
                ctrl_loop_voltage_task(vout_volts);
                vout_cached = vout_volts;
            }

            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_1);
        }
    }
}

/* ==================== 公开 API ==================== */

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
            PFC_V_INTEGRAL_MAX, 0.0f,
            PFC_V_OUTPUT_MAX,   0.0f);

    /* 软启动 */
    state             = CTRL_STATE_SOFT_START;
    vout_target       = PFC_NOMINAL_VOUT;
    vref_ramp         = 0.0f;
    soft_start_ticks  = 0;
    i_amplitude       = 0.0f;
    duty_current      = 0.0f;
    prev_polarity     = 0;
    now_voltage_mV    = 0.0f;
    now_current_A     = 0.0f;
    vout_cached       = 0.0f;

    /* Timer B 安全态: 全关 */
    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP1xR = 0;
    hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP3xR = 0;

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
                           float32_t abs_sin, uint8_t polarity)
{
    if (polarity != prev_polarity) {
        pfc_set_polarity(polarity);
        prev_polarity = polarity;
    }

    float32_t i_ref = pfc_gen_i_ref(i_amplitude, abs_sin);
    float32_t i_error      = i_ref - il_inst;
    float32_t i_correction = pi_compute(&i_pi, i_error);

    float32_t duty_ideal = pfc_calc_ideal_duty(vin_inst, vout);
    duty_current = duty_ideal + i_correction;

    pfc_write_duty(duty_current);
}

/* ---- 电压外环 (~300Hz) ---- */

void ctrl_loop_voltage_task(float32_t vout_measured)
{
    if (state == CTRL_STATE_IDLE || state == CTRL_STATE_FAULT) {
        i_amplitude = 0.0f;
        return;
    }

    if (state == CTRL_STATE_SOFT_START) {
        soft_start_ticks++;
        vref_ramp = vout_target *
                    ((float32_t)soft_start_ticks / (float32_t)PFC_SOFTSTART_STEPS);

        if (soft_start_ticks >= PFC_SOFTSTART_STEPS) {
            vref_ramp = vout_target;
            state = CTRL_STATE_RUNNING;
        }
    }

    float32_t v_error = vref_ramp - vout_measured;
    i_amplitude = pi_compute(&v_pi, v_error);
}

/* ---- 运行时修改目标电压 ---- */

void ctrl_loop_set_vout(float32_t vout)
{
    if (vout < 1.0f) vout = 1.0f;
    vout_target = vout;

    if (state == CTRL_STATE_RUNNING) {
        state = CTRL_STATE_SOFT_START;
        soft_start_ticks = (uint32_t)(vref_ramp / vout_target * PFC_SOFTSTART_STEPS);
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

float32_t ctrl_loop_get_voltage(void)     { return now_voltage_mV / 1000.0f; }
float32_t ctrl_loop_get_current(void)     { return now_current_A; }
float32_t ctrl_loop_get_vout_cached(void) { return vout_cached; }
