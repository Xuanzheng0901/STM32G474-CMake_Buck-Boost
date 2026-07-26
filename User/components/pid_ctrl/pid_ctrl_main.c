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

static pid_ctrl_block_handle_t a_pid_handle = NULL;
static pid_ctrl_block_handle_t b_pid_handle = NULL;
static pid_ctrl_block_handle_t c_pid_handle = NULL;
QueueHandle_t pid_ctrl_queue_mV = NULL; //单位为mV
float now_voltage_mV[3] = {0.0f}; // [0]=Va, [1]=Vb, [2]=Vc (mV)
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


// 把DMA块数据转换为三相电压/电流工程量，并做电压去噪
// adc12_data: ADC1(low 16b) + ADC2(high 16b) 双同步模式
//   - 偶数字: rank1 = Va|Vb, 奇数字: rank2 = Ia|Ib
// adc3_data: ADC3 独立DMA
//   - 偶数字: rank1 = Vc,      奇数字: rank2 = Ic
static void adc_data_process(uint32_t *adc12_data, uint16_t *adc3_data)
{
    static kalman_1d_state_t kf_voltage[3]; // A, B, C 相电压卡尔曼
    static kalman_1d_state_t kf_current[3]; // A, B, C 相电流卡尔曼
    static uint8_t is_kf_initialized = 0;

    // 每半缓冲区的完整采样组数
#define SAMPLE_COUNT (ADC_BUFFER_LENGTH / 4)

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
    // 提取原始ADC值到浮点数组
    float32_t raw_v[3][SAMPLE_COUNT];
    float32_t raw_i[3][SAMPLE_COUNT];

    for(uint16_t i = 0; i < SAMPLE_COUNT; i++)
    {
        // Rank1: 电压
        raw_v[0][i] = (float32_t)(adc12_data[2 * i] & 0x0FFF);         // ADC1 → Va
        raw_v[1][i] = (float32_t)((adc12_data[2 * i] >> 16) & 0x0FFF); // ADC2 → Vb
        raw_v[2][i] = (float32_t)(adc3_data[2 * i] & 0x0FFF);           // ADC3 → Vc

        // Rank2: 电流
        raw_i[0][i] = (float32_t)(adc12_data[2 * i + 1] & 0x0FFF);         // ADC1 → Ia
        raw_i[1][i] = (float32_t)((adc12_data[2 * i + 1] >> 16) & 0x0FFF); // ADC2 → Ib
        raw_i[2][i] = (float32_t)(adc3_data[2 * i + 1] & 0x0FFF);           // ADC3 → Ic
    }

    // 电压/电流 RMS 系数
    // V: 3000mV / 4095 * 39.25 ≈ 28.5
    // I: (3000mV / 4095) / 100 ≈ 0.007326
#define V_RMS_COEF (28.5f)
#define I_RMS_COEF (0.007326007f)

    float32_t mean, power, variance;

    // 逐相计算 AC RMS 并卡尔曼滤波
    for(uint8_t ph = 0; ph < 3; ph++)
    {
        // --- 电压: Var = SumSq/N - Mean² ---
        arm_mean_f32(raw_v[ph], SAMPLE_COUNT, &mean);
        arm_power_f32(raw_v[ph], SAMPLE_COUNT, &power);
        variance = power / (float32_t)SAMPLE_COUNT - mean * mean;
        if(variance < 0.0f)
            variance = 0.0f;
        arm_sqrt_f32(variance, &mean); // 复用 mean 存放 stddev
        float raw_voltage_mV = mean * V_RMS_COEF;

        // --- 电流 ---
        arm_mean_f32(raw_i[ph], SAMPLE_COUNT, &mean);
        arm_power_f32(raw_i[ph], SAMPLE_COUNT, &power);
        variance = power / (float32_t)SAMPLE_COUNT - mean * mean;
        if(variance < 0.0f)
            variance = 0.0f;
        arm_sqrt_f32(variance, &mean);
        float raw_current_A = mean * I_RMS_COEF;

        if(!is_kf_initialized)
        {
            kalman_1d_init(&kf_voltage[ph], raw_voltage_mV, 10.0f, 0.5f, 50.0f);
            kalman_1d_init(&kf_current[ph], raw_current_A, 1.0f, 0.01f, 1.0f);
        }

        now_voltage_mV[ph] = kalman_1d_update(&kf_voltage[ph], raw_voltage_mV);
        now_current_A[ph] = kalman_1d_update(&kf_current[ph], raw_current_A);
    }

    // 三组滤波器的初始状态一次性置位（仅首次执行后）
    if(!is_kf_initialized)
        is_kf_initialized = 1;

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);

#undef SAMPLE_COUNT
}


static void PID_ctrl_routine(void *pvParameters)
{
    static uint32_t target_voltage_mV = 0;
    static uint32_t target_voltage_buffer_mV = 0;

    static float output = 0.0f;

    static adc_dma_block_t dma_block;
    set_mod_ratio_by_factor(0.95f, 0.95f, 0.95f);
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
                    // pid_reset_ctrl_block(pid_handle); //使用增量式pid更改target后不能重置
                }
            }
            adc_data_process(dma_block.adc12_half, dma_block.adc3_half);
            //
            //4. 进行pid计算
            // float error_mV = (float)target_voltage_mV - now_voltage_mV;
            //
            // pid_compute(pid_handle, error_mV, &output);
            // if(output < 0.01f)
            //     output = 0.0f;
            // set_mod_ratio_by_factor(output);
            // LOGI("PID", "output: %.6f", output);
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
    pid_new_control_block(&pid_cfg, &a_pid_handle);
    pid_new_control_block(&pid_cfg, &b_pid_handle);
    pid_new_control_block(&pid_cfg, &c_pid_handle);
    pid_ctrl_queue_mV = xQueueCreate(6, sizeof(uint32_t));
    xTaskCreate(PID_ctrl_routine, "PID", 4096, NULL, 15, NULL);
    pid_set_voltage(0);
}
