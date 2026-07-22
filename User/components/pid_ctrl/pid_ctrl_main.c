#include "pid_ctrl_internal.h"
#include "FreeRTOS.h"
#include <math.h>
#include "hrtim.h"
#include "main.h"
#include "task.h"
#include "queue.h"
#include "kalman.h"

extern QueueHandle_t adc_queue;

static pid_ctrl_block_handle_t pid_handle = NULL;
QueueHandle_t pid_ctrl_queue_mV = NULL; //单位为mV
static float now_current_A = 0.0f, now_voltage_mV = 0.0f;

// --- 定义常量 ---
#define TABLE_SIZE 600

volatile uint32_t SineTable[TABLE_SIZE] = {0};
const float sine_wave[TABLE_SIZE] = {
    0.000000f, 0.010472f, 0.020942f, 0.031411f, 0.041876f, 0.052336f, 0.062791f, 0.073238f, 0.083678f, 0.094108f,
    0.104528f, 0.114937f, 0.125333f, 0.135716f, 0.146083f, 0.156434f, 0.166769f, 0.177085f, 0.187381f, 0.197657f,
    0.207912f, 0.218143f, 0.228351f, 0.238533f, 0.248690f, 0.258819f, 0.268920f, 0.278991f, 0.289032f, 0.299041f,
    0.309017f, 0.318959f, 0.328867f, 0.338738f, 0.348572f, 0.358368f, 0.368125f, 0.377841f, 0.387516f, 0.397148f,
    0.406737f, 0.416281f, 0.425779f, 0.435231f, 0.444635f, 0.453990f, 0.463296f, 0.472551f, 0.481754f, 0.490904f,
    0.500000f, 0.509041f, 0.518027f, 0.526956f, 0.535827f, 0.544639f, 0.553392f, 0.562083f, 0.570714f, 0.579281f,
    0.587785f, 0.596225f, 0.604599f, 0.612907f, 0.621148f, 0.629320f, 0.637424f, 0.645458f, 0.653421f, 0.661312f,
    0.669131f, 0.676876f, 0.684547f, 0.692143f, 0.699663f, 0.707107f, 0.714473f, 0.721760f, 0.728969f, 0.736097f,
    0.743145f, 0.750111f, 0.756995f, 0.763796f, 0.770513f, 0.777146f, 0.783693f, 0.790155f, 0.796530f, 0.802817f,
    0.809017f, 0.815128f, 0.821149f, 0.827081f, 0.832921f, 0.838671f, 0.844328f, 0.849893f, 0.855364f, 0.860742f,
    0.866025f, 0.871214f, 0.876307f, 0.881303f, 0.886204f, 0.891007f, 0.895712f, 0.900319f, 0.904827f, 0.909236f,
    0.913545f, 0.917755f, 0.921863f, 0.925871f, 0.929776f, 0.933580f, 0.937282f, 0.940881f, 0.944376f, 0.947768f,
    0.951057f, 0.954240f, 0.957319f, 0.960294f, 0.963163f, 0.965926f, 0.968583f, 0.971134f, 0.973579f, 0.975917f,
    0.978148f, 0.980271f, 0.982287f, 0.984196f, 0.985996f, 0.987688f, 0.989272f, 0.990748f, 0.992115f, 0.993373f,
    0.994522f, 0.995562f, 0.996493f, 0.997314f, 0.998027f, 0.998630f, 0.999123f, 0.999507f, 0.999781f, 0.999945f,
    1.000000f, 0.999945f, 0.999781f, 0.999507f, 0.999123f, 0.998630f, 0.998027f, 0.997314f, 0.996493f, 0.995562f,
    0.994522f, 0.993373f, 0.992115f, 0.990748f, 0.989272f, 0.987688f, 0.985996f, 0.984196f, 0.982287f, 0.980271f,
    0.978148f, 0.975917f, 0.973579f, 0.971134f, 0.968583f, 0.965926f, 0.963163f, 0.960294f, 0.957319f, 0.954240f,
    0.951057f, 0.947768f, 0.944376f, 0.940881f, 0.937282f, 0.933580f, 0.929776f, 0.925871f, 0.921863f, 0.917755f,
    0.913545f, 0.909236f, 0.904827f, 0.900319f, 0.895712f, 0.891007f, 0.886204f, 0.881303f, 0.876307f, 0.871214f,
    0.866025f, 0.860742f, 0.855364f, 0.849893f, 0.844328f, 0.838671f, 0.832921f, 0.827081f, 0.821149f, 0.815128f,
    0.809017f, 0.802817f, 0.796530f, 0.790155f, 0.783693f, 0.777146f, 0.770513f, 0.763796f, 0.756995f, 0.750111f,
    0.743145f, 0.736097f, 0.728969f, 0.721760f, 0.714473f, 0.707107f, 0.699663f, 0.692143f, 0.684547f, 0.676876f,
    0.669131f, 0.661312f, 0.653421f, 0.645458f, 0.637424f, 0.629320f, 0.621148f, 0.612907f, 0.604599f, 0.596225f,
    0.587785f, 0.579281f, 0.570714f, 0.562083f, 0.553392f, 0.544639f, 0.535827f, 0.526956f, 0.518027f, 0.509041f,
    0.500000f, 0.490904f, 0.481754f, 0.472551f, 0.463296f, 0.453990f, 0.444635f, 0.435231f, 0.425779f, 0.416281f,
    0.406737f, 0.397148f, 0.387516f, 0.377841f, 0.368125f, 0.358368f, 0.348572f, 0.338738f, 0.328867f, 0.318959f,
    0.309017f, 0.299041f, 0.289032f, 0.278991f, 0.268920f, 0.258819f, 0.248690f, 0.238533f, 0.228351f, 0.218143f,
    0.207912f, 0.197657f, 0.187381f, 0.177085f, 0.166769f, 0.156434f, 0.146083f, 0.135716f, 0.125333f, 0.114937f,
    0.104528f, 0.094108f, 0.083678f, 0.073238f, 0.062791f, 0.052336f, 0.041876f, 0.031411f, 0.020942f, 0.010472f,
    0.000000f, -0.010472f, -0.020942f, -0.031411f, -0.041876f, -0.052336f, -0.062791f, -0.073238f, -0.083678f,
    -0.094108f,
    -0.104528f, -0.114937f, -0.125333f, -0.135716f, -0.146083f, -0.156434f, -0.166769f, -0.177085f, -0.187381f,
    -0.197657f,
    -0.207912f, -0.218143f, -0.228351f, -0.238533f, -0.248690f, -0.258819f, -0.268920f, -0.278991f, -0.289032f,
    -0.299041f,
    -0.309017f, -0.318959f, -0.328867f, -0.338738f, -0.348572f, -0.358368f, -0.368125f, -0.377841f, -0.387516f,
    -0.397148f,
    -0.406737f, -0.416281f, -0.425779f, -0.435231f, -0.444635f, -0.453990f, -0.463296f, -0.472551f, -0.481754f,
    -0.490904f,
    -0.500000f, -0.509041f, -0.518027f, -0.526956f, -0.535827f, -0.544639f, -0.553392f, -0.562083f, -0.570714f,
    -0.579281f,
    -0.587785f, -0.596225f, -0.604599f, -0.612907f, -0.621148f, -0.629320f, -0.637424f, -0.645458f, -0.653421f,
    -0.661312f,
    -0.669131f, -0.676876f, -0.684547f, -0.692143f, -0.699663f, -0.707107f, -0.714473f, -0.721760f, -0.728969f,
    -0.736097f,
    -0.743145f, -0.750111f, -0.756995f, -0.763796f, -0.770513f, -0.777146f, -0.783693f, -0.790155f, -0.796530f,
    -0.802817f,
    -0.809017f, -0.815128f, -0.821149f, -0.827081f, -0.832921f, -0.838671f, -0.844328f, -0.849893f, -0.855364f,
    -0.860742f,
    -0.866025f, -0.871214f, -0.876307f, -0.881303f, -0.886204f, -0.891007f, -0.895712f, -0.900319f, -0.904827f,
    -0.909236f,
    -0.913545f, -0.917755f, -0.921863f, -0.925871f, -0.929776f, -0.933580f, -0.937282f, -0.940881f, -0.944376f,
    -0.947768f,
    -0.951057f, -0.954240f, -0.957319f, -0.960294f, -0.963163f, -0.965926f, -0.968583f, -0.971134f, -0.973579f,
    -0.975917f,
    -0.978148f, -0.980271f, -0.982287f, -0.984196f, -0.985996f, -0.987688f, -0.989272f, -0.990748f, -0.992115f,
    -0.993373f,
    -0.994522f, -0.995562f, -0.996493f, -0.997314f, -0.998027f, -0.998630f, -0.999123f, -0.999507f, -0.999781f,
    -0.999945f,
    -1.000000f, -0.999945f, -0.999781f, -0.999507f, -0.999123f, -0.998630f, -0.998027f, -0.997314f, -0.996493f,
    -0.995562f,
    -0.994522f, -0.993373f, -0.992115f, -0.990748f, -0.989272f, -0.987688f, -0.985996f, -0.984196f, -0.982287f,
    -0.980271f,
    -0.978148f, -0.975917f, -0.973579f, -0.971134f, -0.968583f, -0.965926f, -0.963163f, -0.960294f, -0.957319f,
    -0.954240f,
    -0.951057f, -0.947768f, -0.944376f, -0.940881f, -0.937282f, -0.933580f, -0.929776f, -0.925871f, -0.921863f,
    -0.917755f,
    -0.913545f, -0.909236f, -0.904827f, -0.900319f, -0.895712f, -0.891007f, -0.886204f, -0.881303f, -0.876307f,
    -0.871214f,
    -0.866025f, -0.860742f, -0.855364f, -0.849893f, -0.844328f, -0.838671f, -0.832921f, -0.827081f, -0.821149f,
    -0.815128f,
    -0.809017f, -0.802817f, -0.796530f, -0.790155f, -0.783693f, -0.777146f, -0.770513f, -0.763796f, -0.756995f,
    -0.750111f,
    -0.743145f, -0.736097f, -0.728969f, -0.721760f, -0.714473f, -0.707107f, -0.699663f, -0.692143f, -0.684547f,
    -0.676876f,
    -0.669131f, -0.661312f, -0.653421f, -0.645458f, -0.637424f, -0.629320f, -0.621148f, -0.612907f, -0.604599f,
    -0.596225f,
    -0.587785f, -0.579281f, -0.570714f, -0.562083f, -0.553392f, -0.544639f, -0.535827f, -0.526956f, -0.518027f,
    -0.509041f,
    -0.500000f, -0.490904f, -0.481754f, -0.472551f, -0.463296f, -0.453990f, -0.444635f, -0.435231f, -0.425779f,
    -0.416281f,
    -0.406737f, -0.397148f, -0.387516f, -0.377841f, -0.368125f, -0.358368f, -0.348572f, -0.338738f, -0.328867f,
    -0.318959f,
    -0.309017f, -0.299041f, -0.289032f, -0.278991f, -0.268920f, -0.258819f, -0.248690f, -0.238533f, -0.228351f,
    -0.218143f,
    -0.207912f, -0.197657f, -0.187381f, -0.177085f, -0.166769f, -0.156434f, -0.146083f, -0.135716f, -0.125333f,
    -0.114937f,
    -0.104528f, -0.094108f, -0.083678f, -0.073238f, -0.062791f, -0.052336f, -0.041876f, -0.031411f, -0.020942f,
    -0.010472f,
};
static const uint32_t hp = PWM_Period / 2;

void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t TimerIdx)
{
    static uint16_t sin_index = 0;
    if(TimerIdx != HRTIM_TIMERINDEX_MASTER)
        return;

    GPIOC->ODR ^= GPIO_PIN_1;

    uint16_t idx_b = sin_index + 200;
    uint16_t idx_c = sin_index + 400;
    if(idx_b >= 600)
        idx_b -= 600;
    if(idx_c >= 600)
        idx_c -= 600;


    HRTIM_Timerx_TypeDef *const tx = hhrtim->Instance->sTimerxRegs;

    uint32_t cmp1;
    cmp1 = SineTable[sin_index];
    tx[0].CMP1xR = cmp1;
    tx[0].CMP3xR = PWM_Period - cmp1;

    cmp1 = SineTable[idx_b];
    tx[1].CMP1xR = cmp1;
    tx[1].CMP3xR = PWM_Period - cmp1;

    cmp1 = SineTable[idx_c];
    tx[2].CMP1xR = cmp1;
    tx[2].CMP3xR = PWM_Period - cmp1;

    sin_index++;
    if(sin_index >= 600)
        sin_index = 0;

    GPIOC->ODR ^= GPIO_PIN_1;
}

static void set_mod_ratio_by_factor(float factor)
{
    for(int i = 0; i < TABLE_SIZE; i++)
    {
        SineTable[i] = (uint32_t)((float)hp * (1.0f + factor * sine_wave[i]) + 0.5f);
        SineTable[i] = hp - (SineTable[i] >> 1);
    }
}

float get_voltage_value(uint8_t index)
{
    if(index == 0)
        return now_voltage_mV;
    if(index == 1)
        return now_current_A;

    return 0.0f;
}

// static float last_last_voltage_mV = 0.0f;
// static float last_last_current_A = 0.0f;

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
    set_mod_ratio_by_factor(0.95f);
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
                    // pid_reset_ctrl_block(pid_handle); //使用增量式pid更改target后不能重置
                }
            }
            adc_data_process(buf_ptr);
            //
            //4. 进行pid计算
            // float error_mV = (float)target_voltage_mV - now_voltage_mV;
            //
            // pid_compute(pid_handle, error_mV, &output);
            // if(output < 0.01f)
            //     output = 0.0f;
            // set_mod_ratio_by_factor(output);
            // LOGI("PID", "output: %.6f", output);
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
