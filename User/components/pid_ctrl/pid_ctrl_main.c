#include "pid_ctrl_internal.h"
#include "FreeRTOS.h"
#include "hrtim.h"
#include "main.h"
#include "task.h"
#include "queue.h"
#include "kalman.h"

extern QueueHandle_t adc_queue;

static pid_ctrl_block_handle_t v_pid_handle = NULL;
static pid_ctrl_block_handle_t i_pid_handle = NULL;
QueueHandle_t pid_ctrl_queue_mV = NULL; //单位为mV
QueueHandle_t pid_ctrl_queue_mA = NULL; //单位为mA (限流)
static float now_current_mA = 0.0f, now_voltage_mV = 0.0f;

// --- 定义常量 ---
#define PWM_PERIOD_ARR    PWM_Period
#define INPUT_VOLTAGE_MV  30000.0f // 输入电压 (暂时假设30V)
#define MAX_DUTY_RATIO    0.80f    // 最大占空比限制 (理论4倍升压)
#define RC_ALPHA_DENO_1   0.85f
#define RC_ALPHA_DENO_2   0.10f
#define RC_ALPHA_DENO_3   0.05f

static uint32_t voltage_to_duty(float expect_mV)
{
    if(expect_mV < 10.0f)
        return 0;

    float denominator = expect_mV + INPUT_VOLTAGE_MV;

    if(denominator < 100.0f)
        denominator = 100.0f;

    float duty_cycle = expect_mV / denominator;

    // 硬件保护
    if(duty_cycle > MAX_DUTY_RATIO)
        duty_cycle = MAX_DUTY_RATIO;

    return (uint32_t)(duty_cycle * PWM_PERIOD_ARR);
}

void pwm_set_duty(uint32_t pwm_duty) //占空比0-54400
{
    if(pwm_duty <= 27200)
    {
        __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_COMPAREUNIT_3, 27200 - (pwm_duty >> 1) + 1);
    }
    else
    {
        __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_COMPAREUNIT_3, (pwm_duty >> 1) + 1);
    }

    hhrtim1.Instance->sTimerxRegs[TIMER_E].CMP1CxR = pwm_duty;
}

float get_voltage_value(uint8_t index)
{
    if(index == 0)
        return now_voltage_mV;
    if(index == 1)
        return now_current_mA;

    return 0.0f;
}

static float last_last_voltage_mV = 0.0f;
static float last_last_current_A = 0.0f;

// 把DMA块数据转换为电压/电流工程量，并做电压去噪
static inline void adc_data_process(uint32_t *data_buf)
{
    static uint8_t is_kf_initialized = 0;
    static kalman_1d_state_t kf_voltage;
    static kalman_1d_state_t kf_current;

    float origin_voltage_sum = 0.0f, origin_current_sum = 0.0f;
    for(uint8_t i = 0; i < ADC_BUFFER_LENGTH / 2; i++)
    {
        origin_voltage_sum += data_buf[i] & 0x0FFF;
        origin_current_sum += 2254 - (int32_t)(data_buf[i] >> 16);
    }
    // origin_voltage_sum / 4095 * 3300 / 20(样本量) = adc引脚上的电压
    // * 20 = 运放输入电压(运放20分压)
    // * 1.0079(adc误差修正)
    float temp_voltage_mV = origin_voltage_sum * 3000.0f / 4095.0f / (float)(ADC_BUFFER_LENGTH / 2) * 20.0f * 1.0048f;
    if(temp_voltage_mV < 0.0f)
    {
        temp_voltage_mV = 0.0f;
    }

    float temp_current_mA = origin_current_sum * 3000.0f / 4095.0f / (float)(ADC_BUFFER_LENGTH / 2) * 2.0f;//单位mA

    //对均值进行卡尔曼滤波
    if(!is_kf_initialized)
    {
        // 参数调整说明：
        // Q越大，跟踪越快，滤波效果越弱；Q越小，系统越稳定，但存在滞后
        // R越大，滤波效果越强，认为传感器噪声大；R越小，越相信传感器测量值
        kalman_1d_init(&kf_voltage, temp_voltage_mV, 10.0f, 0.5f, 50.0f); // 电压Q=0.5, R=50
        kalman_1d_init(&kf_current, temp_current_mA, 1.0f, 0.01f, 1.0f); // 电流Q=0.01, R=1.0
        is_kf_initialized = 1;
    }

    // 3. 执行滤波，覆盖全局变量
    now_voltage_mV = kalman_1d_update(&kf_voltage, temp_voltage_mV);
    now_current_mA = kalman_1d_update(&kf_current, temp_current_mA);
}


static void PID_ctrl_routine(void *pvParameters)
{
    static uint32_t target_voltage_mV = 0;
    static uint32_t target_voltage_buffer_mV = 0;

    static uint32_t target_current_mA = 1000; // 默认限流1000mA
    static uint32_t target_current_buffer_mA = 0;

    static float next_output_voltage_mV = 0.0f;

    static uint32_t *buf_ptr;
    while(1)
    {
        //1. 等待ADC数据
        if(xQueueReceive(adc_queue, &buf_ptr, portMAX_DELAY) == pdTRUE)
        {
            //2. 查询target是否改变
            if(pdPASS == xQueueReceive(pid_ctrl_queue_mV, &target_voltage_buffer_mV, 0))
            {
                if(target_voltage_mV != target_voltage_buffer_mV)
                {
                    target_voltage_mV = target_voltage_buffer_mV;
                }
            }
            if(pdPASS == xQueueReceive(pid_ctrl_queue_mA, &target_current_buffer_mA, 0))
            {
                if(target_current_mA != target_current_buffer_mA)
                {
                    target_current_mA = target_current_buffer_mA;
                }
            }

            //3. 数据处理
            adc_data_process(buf_ptr);

            //4. 进行双环pid计算 (使用增量式PID并取最小值的方式实现CV/CC切换)
            float error_mV = (float)target_voltage_mV - now_voltage_mV;
            float error_mA = (float)target_current_mA - now_current_mA;

            float v_pid_out = 0.0f;
            float i_pid_out = 0.0f;

            // 计算电压环输出
            pid_compute(v_pid_handle, error_mV, &v_pid_out);
            // 计算电流环输出
            pid_compute(i_pid_handle, error_mA, &i_pid_out);

            // 比较并选取较小的输出作为最终目标输出 (CV/CC 限流控制)
            if(i_pid_out < v_pid_out)
            {
                // CC 恒流模式接管
                next_output_voltage_mV = i_pid_out;
                // 防止电压环出现积分饱和（跟踪恒流输出）
                pid_set_tracking_output(v_pid_handle, next_output_voltage_mV);
            }
            else
            {
                // CV 恒压模式
                next_output_voltage_mV = v_pid_out;
                // 防止电流环出现积分饱和（跟踪恒压输出）
                pid_set_tracking_output(i_pid_handle, next_output_voltage_mV);
            }

            uint32_t output_duty = voltage_to_duty(next_output_voltage_mV);
            pwm_set_duty(output_duty);
        }
    }
}

void pid_set_voltage(uint32_t mv)
{
    if(NULL == pid_ctrl_queue_mV)
        return;
    xQueueSend(pid_ctrl_queue_mV, &mv, portMAX_DELAY);
}

void pid_set_current_limit(uint32_t ma)
{
    if(NULL == pid_ctrl_queue_mA)
        return;
    xQueueSend(pid_ctrl_queue_mA, &ma, portMAX_DELAY);
}

void pid_ctrl_init(void)
{
    pid_ctrl_config_t pid_cfg = {
        .init_param = {
            .kp           = 0.1f,
            .ki           = 0.06f,
            .kd           = 0.06f,
            .max_output   = 90000.0f,
            .min_output   = 0.0f,
            .max_integral = 1000000.0f,
            .min_integral = -1000000.0f,
            .cal_type     = PID_CAL_TYPE_INCREMENTAL,
        }
    };
    pid_ctrl_config_t i_pid_cfg = pid_cfg;
    // 电流环参数可以根据实际系统响应做调节，这里初始化一个参考值
    i_pid_cfg.init_param.kp = 0.5f;
    i_pid_cfg.init_param.ki = 0.1f;
    i_pid_cfg.init_param.kd = 0.0f;

    pid_new_control_block(&pid_cfg, &v_pid_handle);
    pid_new_control_block(&i_pid_cfg, &i_pid_handle);

    pid_ctrl_queue_mV = xQueueCreate(6, sizeof(uint32_t));
    pid_ctrl_queue_mA = xQueueCreate(6, sizeof(uint32_t));

    xTaskCreate(PID_ctrl_routine, "PID", 2048, NULL, 15, NULL);
    pid_set_voltage(0);
    pid_set_current_limit(1000); // 默认限流 1A
}
