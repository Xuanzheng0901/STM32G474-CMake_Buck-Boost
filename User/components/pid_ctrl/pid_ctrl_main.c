#include "pid_ctrl_internal.h"
#include "FreeRTOS.h"
#include <math.h>
#include "hrtim.h"
#include "main.h"
#include "task.h"
#include "queue.h"
#include "kalman.h"
#include "sogi.h"
#include "ctrl_loop.h"

extern QueueHandle_t adc_queue;

static pid_ctrl_block_handle_t pid_handle = NULL;
QueueHandle_t pid_ctrl_queue_mV = NULL; //单位为mV
static float now_current_A = 0.0f, now_voltage_mV = 0.0f;

void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t TimerIdx)
{
    static uint16_t sin_index = 0;
    if(TimerIdx != HRTIM_TIMERINDEX_MASTER)
        return;

    GPIOC->ODR ^= GPIO_PIN_1;

    GPIOC->ODR ^= GPIO_PIN_1;
}


float get_voltage_value(uint8_t index)
{
    if(index == 0)
        return now_voltage_mV;
    if(index == 1)
        return now_current_A;

    return 0.0f;
}

// 把DMA块数据转换为电压/电流工程量，并做电压去噪
static void adc_data_process(uint32_t *data_buf)
{
    static kalman_1d_state_t kf_voltage;
    static kalman_1d_state_t kf_current;
    static uint8_t is_kf_initialized = 0;

    uint32_t v_sum = 0, i_sum = 0;
    uint64_t v_sq_sum = 0, i_sq_sum = 0; // 使用 64 位防止平方和溢出

    uint16_t len = ADC_BUFFER_LENGTH / 2;

    // 单次循环，全程整数运算，速度极快
    for(uint16_t i = 0; i < len; i++)
    {
        uint32_t v_raw = data_buf[i] & 0x0FFF;
        uint32_t i_raw = data_buf[i] >> 16;

        v_sum += v_raw;
        i_sum += i_raw;

        v_sq_sum += v_raw * v_raw;
        i_sq_sum += i_raw * i_raw;
    }

    // 循环外再转为浮点预算
    float f_len = (float)len;

    // 利用公式 Variance = (SumSq - (Sum * Sum) / N) / N
    float v_var = ((float)v_sq_sum - ((float)v_sum * v_sum) / f_len) / f_len;
    float i_var = ((float)i_sq_sum - ((float)i_sum * i_sum) / f_len) / f_len;

    // 浮点精度可能导致微小的负数，防御性置零
    if(v_var < 0.0f)
        v_var = 0.0f;
    if(i_var < 0.0f)
        i_var = 0.0f;

    // 常数可以在预编译期计算，避免运行时产生多余除法
    // V coef = 3000.0f / 4095.0f * 39.25f;
    // I coef = (3000.0f / 4095.0f) / 100.0f;
#define V_RMS_COEF (28.5f)
#define I_RMS_COEF (0.007326007f)

    // now_voltage_mV = sqrtf(v_var) * V_RMS_COEF;
    // now_current_A = sqrtf(i_var) * I_RMS_COEF;

    // 1. 获取本次测量的原始值
    float raw_voltage_mV = sqrtf(v_var) * V_RMS_COEF;
    float raw_current_A = sqrtf(i_var) * I_RMS_COEF;

    // 2. 动态初始化卡尔曼滤波器 (仅第1次执行)
    // 使用第一次测量值作为初始状态可以加快滤波器的收敛速度
    if(!is_kf_initialized)
    {
        // 参数调整说明：
        // Q越大，跟踪越快，滤波效果越弱；Q越小，系统越稳定，但存在滞后
        // R越大，滤波效果越强，认为传感器噪声大；R越小，越相信传感器测量值
        kalman_1d_init(&kf_voltage, raw_voltage_mV, 10.0f, 0.5f, 50.0f); // 电压Q=0.5, R=50
        kalman_1d_init(&kf_current, raw_current_A, 1.0f, 0.01f, 1.0f); // 电流Q=0.01, R=1.0
        is_kf_initialized = 1;
    }

    // 3. 执行滤波，覆盖全局变量
    now_voltage_mV = kalman_1d_update(&kf_voltage, raw_voltage_mV);
    now_current_A = kalman_1d_update(&kf_current, raw_current_A);
}


static void PID_ctrl_routine(void *pvParameters)
{
    static uint32_t target_voltage_mV = 0;
    static uint32_t target_voltage_buffer_mV = 0;

    static float output = 0.0f;

    static uint32_t *buf_ptr;

    /* 电压外环分频计数器: 每 5 次 ADC 数据处理执行一次 (~300Hz) */
    static uint32_t vloop_div = 0;

    extern void adc_set_vout_cache(float vout);

    while(1)
    {
        //1. 等待ADC数据
        if(xQueueReceive(adc_queue, &buf_ptr, portMAX_DELAY) == pdTRUE)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_1);
            //2. 查询target是否改变
            if(pdPASS == xQueueReceive(pid_ctrl_queue_mV, &target_voltage_buffer_mV, 0))
            {
                if(target_voltage_mV != target_voltage_buffer_mV)
                {
                    target_voltage_mV = target_voltage_buffer_mV;
                }
            }
            adc_data_process(buf_ptr);

            /* ---- PFC 电压外环 (~300Hz) ---- */
            vloop_div++;
            if(vloop_div >= 5)
            {
                vloop_div = 0;
                /* 转换 mV → V, 供电压环使用 */
                float32_t vout_volts = now_voltage_mV / 1000.0f;
                ctrl_loop_voltage_task(vout_volts);
                /* 同步更新 ISR 中电流环使用的 Vout 缓存 */
                adc_set_vout_cache(vout_volts);
            }

            //4. 进行pid计算 (PFC 电压环已替代此逻辑)
            // float error_mV = (float)target_voltage_mV - now_voltage_mV;
            // pid_compute(pid_handle, error_mV, &output);
            // if(output < 0.01f)
            //     output = 0.0f;
            // set_mod_ratio_by_factor(output);
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_1);
        }
    }
}

void pid_set_voltage(uint32_t mv)
{
    if(NULL == pid_ctrl_queue_mV)
        return;
    xQueueSend(pid_ctrl_queue_mV, &mv, portMAX_DELAY);
}

void pid_ctrl_init(void)
{
    pid_ctrl_config_t pid_cfg = {
        .init_param = {
            .kp           = 0.00005f,
            .ki           = 0.000005f,
            .kd           = 0.00006f,
            .max_output   = 0.96f,
            .min_output   = 0.0f,
            .max_integral = 1000000.0f,
            .min_integral = -1000000.0f,
            .cal_type     = PID_CAL_TYPE_INCREMENTAL,
        }
    };
    pid_new_control_block(&pid_cfg, &pid_handle);
    pid_ctrl_queue_mV = xQueueCreate(6, sizeof(uint32_t));
    xTaskCreate(PID_ctrl_routine, "PID", 2048, NULL, 15, NULL);
    pid_set_voltage(0);
}
