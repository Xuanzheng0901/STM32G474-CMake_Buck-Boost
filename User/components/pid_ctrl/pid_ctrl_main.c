#include "pid_ctrl_internal.h"
#include "FreeRTOS.h"
#include "arm_math.h"
#include "hrtim.h"
#include "main.h"
#include "task.h"
#include "queue.h"
#include "kalman.h"
#include "PID.h"

extern QueueHandle_t adc_queue;

static pid_ctrl_block_handle_t line_voltage_pid;
QueueHandle_t pid_ctrl_queue_mV = NULL; // 线电压有效值，单位为mV
float now_voltage_mV[3] = {0.0f}; // [0]=Vab, [1]=Vbc, [2]=Vca (mV)
float now_current_A[3] = {0.0f}; // [0]=Ia, [1]=Ib, [2]=Ic (A)

// --- 定义常量 ---
#define TABLE_SIZE 600

static uint32_t a_sine_table[TABLE_SIZE] = {0};
static uint32_t b_sine_table[TABLE_SIZE] = {0};
static uint32_t c_sine_table[TABLE_SIZE] = {0};

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

    // GPIOC->ODR ^= GPIO_PIN_1;

    uint16_t idx_b = sin_index + 200;
    uint16_t idx_c = sin_index + 400;
    if(idx_b >= 600)
        idx_b -= 600;
    if(idx_c >= 600)
        idx_c -= 600;


    HRTIM_Timerx_TypeDef *const tx = hhrtim->Instance->sTimerxRegs;

    uint32_t cmp1;
    cmp1 = a_sine_table[sin_index];
    tx[0].CMP1xR = cmp1;
    tx[0].CMP3xR = PWM_Period - cmp1;

    cmp1 = b_sine_table[idx_b];
    tx[1].CMP1xR = cmp1;
    tx[1].CMP3xR = PWM_Period - cmp1;

    cmp1 = c_sine_table[idx_c];
    tx[2].CMP1xR = cmp1;
    tx[2].CMP3xR = PWM_Period - cmp1;

    sin_index++;
    if(sin_index >= 600)
        sin_index = 0;

    // GPIOC->ODR ^= GPIO_PIN_1;
}

float32_t temp[TABLE_SIZE];

static void set_mod_ratio_by_factor(float factor_a, float factor_b, float factor_c)
{
    uint32_t *tables[3] = {a_sine_table, b_sine_table, c_sine_table};
    float32_t factors[3] = {factor_a, factor_b, factor_c};
    float32_t scale = (float32_t)hp;

    for(int ph = 0; ph < 3; ph++)
    {
        // temp = hp * factor * sine_wave
        arm_scale_f32(sine_wave, factors[ph] * scale, temp, TABLE_SIZE);
        // temp = hp + hp * factor * sine_wave
        arm_offset_f32(temp, scale, temp, TABLE_SIZE);

        // 量化并计算SPWM比较值
        for(int i = 0; i < TABLE_SIZE; i++)
        {
            tables[ph][i] = (uint32_t)(temp[i] + 0.5f);
            tables[ph][i] = hp - (tables[ph][i] >> 1);
        }
    }
}

float get_voltage_value(uint8_t index)
{
    if(index < 3)
        return now_voltage_mV[index];
    if(index < 6)
        return now_current_A[index - 3];

    return 0.0f;
}


// 把DMA块数据转换为三路线电压/相电流工程量，并做电压去噪
// adc12_data: ADC1(low 16b) + ADC2(high 16b) 双同步模式
//   - 偶数字: rank1 = Vab|Vbc, 奇数字: rank2 = Ia|Ib
// adc3_data: ADC3 独立DMA
//   - 偶数字: rank1 = Vca,      奇数字: rank2 = Ic
static void adc_data_process(uint32_t *adc12_data, uint16_t *adc3_data)
{
    static kalman_1d_state_t kf_voltage[3]; // AB, BC, CA 线电压卡尔曼
    static kalman_1d_state_t kf_current[3]; // A, B, C 相电流卡尔曼
    static uint8_t is_kf_initialized = 0;

    // 每半缓冲区的完整采样组数
#define SAMPLE_COUNT (ADC_BUFFER_LENGTH / 4)

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);

    // 整数累加器: 单次遍历提取 ADC 值并累加 sum / sum_sq
    // 消除原来 2400 字节的二维浮点数组和 12 次 CMSIS-DSP 遍历
    int64_t sum_v[3] = {0};
    int64_t sum_i[3] = {0};
    int64_t sum_sq_v[3] = {0};
    int64_t sum_sq_i[3] = {0};

    for(uint16_t i = 0; i < SAMPLE_COUNT; i++)
    {
        int32_t raw;

        // Rank1: 电压
        raw = (int32_t)(adc12_data[2 * i] & 0x0FFF);
        sum_v[0] += raw;
        sum_sq_v[0] += (int64_t)raw * raw;
        raw = (int32_t)((adc12_data[2 * i] >> 16) & 0x0FFF);
        sum_v[1] += raw;
        sum_sq_v[1] += (int64_t)raw * raw;
        raw = (int32_t)(adc3_data[2 * i] & 0x0FFF);
        sum_v[2] += raw;
        sum_sq_v[2] += (int64_t)raw * raw;

        // Rank2: 电流
        raw = (int32_t)(adc12_data[2 * i + 1] & 0x0FFF);
        sum_i[0] += raw;
        sum_sq_i[0] += (int64_t)raw * raw;
        raw = (int32_t)((adc12_data[2 * i + 1] >> 16) & 0x0FFF);
        sum_i[1] += raw;
        sum_sq_i[1] += (int64_t)raw * raw;
        raw = (int32_t)(adc3_data[2 * i + 1] & 0x0FFF);
        sum_i[2] += raw;
        sum_sq_i[2] += (int64_t)raw * raw;
    }

    // 预计算倒数，用乘法替代 6 次除法
    const float inv_N = 1.0f / (float)SAMPLE_COUNT;

    // 三路线电压 RMS 系数。分别按 28.5 * 示波器实测值 / MCU测量值进行标定。
    // 相电压通道原有校准值不能直接用于重新接线后的线电压通道。
    static const float voltage_rms_coef[3] = {
        28.5f, // Vab
        28.5f, // Vbc
        28.5f, // Vca
    };

    // 电流 RMS 系数
    // I: (3000mV / 4095) / 100 ≈ 0.007326
#define I_RMS_COEF (0.007326007f)

    // 一次性转浮点计算 RMS + 卡尔曼滤波
    for(uint8_t ph = 0; ph < 3; ph++)
    {
        // --- 电压: Var = SumSq/N - Mean² ---
        float f_sum = (float)sum_v[ph];
        float f_sq = (float)sum_sq_v[ph];
        float mean = f_sum * inv_N;
        float variance = f_sq * inv_N - mean * mean;
        if(variance < 0.0f)
            variance = 0.0f;
        float rms;
        arm_sqrt_f32(variance, &rms);
        float raw_voltage_mV = rms * voltage_rms_coef[ph];

        // --- 电流 ---
        f_sum = (float)sum_i[ph];
        f_sq = (float)sum_sq_i[ph];
        mean = f_sum * inv_N;
        variance = f_sq * inv_N - mean * mean;
        if(variance < 0.0f)
            variance = 0.0f;
        arm_sqrt_f32(variance, &rms);
        float raw_current_A = rms * I_RMS_COEF;

        if(!is_kf_initialized)
        {
            kalman_1d_init(&kf_voltage[ph], raw_voltage_mV, 10.0f, 0.5f, 50.0f);
            kalman_1d_init(&kf_current[ph], raw_current_A, 1.0f, 0.01f, 1.0f);
        }

        now_voltage_mV[ph] = kalman_1d_update(&kf_voltage[ph], raw_voltage_mV);
        now_current_A[ph] = kalman_1d_update(&kf_current[ph], raw_current_A);
    }

    if(!is_kf_initialized)
        is_kf_initialized = 1;

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);

#undef SAMPLE_COUNT
}


static void PID_ctrl_routine(void *pvParameters)
{
    static uint32_t target_voltage_mV = 0;
    static uint32_t target_voltage_buffer_mV = 0;
    static float modulation;

    static adc_dma_block_t dma_block;
    set_mod_ratio_by_factor(0.0f, 0.0f, 0.0f);
    while(1)
    {
        //1. 等待ADC数据
        if(xQueueReceive(adc_queue, &dma_block, portMAX_DELAY) == pdTRUE)
        {
            //2. 查询target是否改变
            if(pdPASS == xQueueReceive(pid_ctrl_queue_mV, &target_voltage_buffer_mV, 0))
            {
                if(target_voltage_mV != target_voltage_buffer_mV)
                {
                    target_voltage_mV = target_voltage_buffer_mV;
                }
            }

            //3. adc数据处理
            adc_data_process(dma_block.adc12_half, dma_block.adc3_half);

            // 线电压彼此耦合，使用三路线电压均值驱动一个公共电压环。
            // 三个桥臂共用同一调制度，保持三相幅值一致和严格120°相位差。
            const float line_voltage_feedback_mV =
                    (now_voltage_mV[0] + now_voltage_mV[1] + now_voltage_mV[2]) / 3.0f;
            const float error_mV = (float)target_voltage_mV - line_voltage_feedback_mV;
            pid_compute(line_voltage_pid, error_mV, &modulation);
            set_mod_ratio_by_factor(modulation, modulation, modulation);

            LOGI("PID", "Vll=%.1fmV, mod=%.3f", line_voltage_feedback_mV, modulation);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
        }
    }
}

void pid_set_voltage(uint32_t mv)
{
    if(NULL == pid_ctrl_queue_mV)
        return;
    xQueueSend(pid_ctrl_queue_mV, &mv, portMAX_DELAY);
}

void pid_set_param(float kp, float ki, float kd)
{
    pid_ctrl_parameter_t param = {
        .kp           = kp,
        .ki           = ki,
        .kd           = kd,
        .max_output   = 0.98f,
        .min_output   = 0.0f,
        .max_integral = 1000000.0f,
        .min_integral = -1000000.0f,
        .cal_type     = PID_CAL_TYPE_POSITIONAL,
    };
    pid_update_parameters(line_voltage_pid, &param);
}

void pid_ctrl_init(void)
{
    pid_ctrl_config_t pid_cfg = {
        .init_param = {
            // 线电压对象的增益约为相电压对象的sqrt(3)倍。
            .kp           = 0.000057735f,
            .ki           = 0.000006928f,
            .kd           = 0.000028868f,
            .max_output   = 0.98f,
            .min_output   = 0.0f,
            .max_integral = 1000000.0f,
            .min_integral = -1000000.0f,
            .cal_type     = PID_CAL_TYPE_POSITIONAL,
        }
    };
    pid_new_control_block(&pid_cfg, &line_voltage_pid);
    pid_reset_ctrl_block(line_voltage_pid);

    pid_ctrl_queue_mV = xQueueCreate(6, sizeof(uint32_t));
    xTaskCreate(PID_ctrl_routine, "PID", 2048, NULL, 15, NULL);
    pid_set_voltage(0);
}
