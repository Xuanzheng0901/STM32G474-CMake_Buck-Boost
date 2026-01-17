#include "pid_ctrl_internal.h"
#include "FreeRTOS.h"
#include "hrtim.h"
#include "main.h"
#include "task.h"
#include "queue.h"

extern QueueHandle_t adc_queue;

static pid_ctrl_block_handle_t pid_handle = NULL;
QueueHandle_t pid_ctrl_queue_mV = NULL; //单位为mV
static float now_current_A = 0.0f, now_voltage_mV = 0.0f;

// --- 定义常量 ---
#define PWM_PERIOD_ARR    PWM_Period
#define INPUT_VOLTAGE_MV  30000.0f // 输入电压 (暂时假设30V)
#define MAX_DUTY_RATIO    0.80f    // 最大占空比限制 (理论4倍升压)

static uint32_t voltage_to_duty(float expect_mV)
{
    if(expect_mV < 0.1f)
        return 0;

    float denominator = expect_mV + INPUT_VOLTAGE_MV;
    float duty_cycle = 0.0f;

    if(denominator > 1.0f)
        duty_cycle = expect_mV / denominator;

    // 硬件保护
    if(duty_cycle > MAX_DUTY_RATIO)
        duty_cycle = MAX_DUTY_RATIO;

    return (uint32_t)(duty_cycle * PWM_PERIOD_ARR);
}

void pwm_set_duty(uint32_t pwm_duty)
{
    hhrtim1.Instance->sTimerxRegs[4].CMP1CxR = pwm_duty;
}

float get_voltage_value(uint8_t index)
{
    if(index == 0)
        return now_voltage_mV;
    if(index == 1)
        return now_current_A;

    return 0.0f;
}


static void PID_ctrl_routine(void *pvParameters)
{
    static uint32_t target_voltage_mV = 0;
    static uint32_t target_voltage_buffer_mV = 0;

    static float next_output_voltage_mV = 0.0f;

    static uint32_t *buf_ptr;

    while(1)
    {
        if(xQueueReceive(adc_queue, &buf_ptr, portMAX_DELAY) == pdTRUE)
        {
            if(pdPASS == xQueueReceive(pid_ctrl_queue_mV, &target_voltage_buffer_mV, 0))
            {
                if(target_voltage_mV != target_voltage_buffer_mV)
                {
                    target_voltage_mV = target_voltage_buffer_mV;
                    pid_reset_ctrl_block(pid_handle);
                }
            }

            float origin_voltage_sum = 0.0f, origin_current_sum = 0.0f;
            for(uint8_t i = 0; i < ADC_BUFFER_LENGTH / 2; i++)
            {
                origin_voltage_sum += buf_ptr[i] & 0xFFFF;
                origin_current_sum += buf_ptr[i] >> 16;
            }
            //                                        读数平均值               读取到的电压  20分压
            now_voltage_mV = origin_voltage_sum / (ADC_BUFFER_LENGTH / 2) / 4095.0f * 3300.0f * 20;
            now_current_A = origin_current_sum / (ADC_BUFFER_LENGTH / 2) / 4095.0f * 3300.0f * 2 / 1000; //单位A

            // float error_mV = (float)target_voltage_mV - now_voltage_mV;
            //
            // pid_compute(pid_handle, error_mV, &next_output_voltage_mV);
            // uint32_t output_duty = voltage_to_duty(next_output_voltage_mV);
            // pwm_set_duty(output_duty);
        }
        // vTaskDelay(1);
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
            .kp = 0.05f,
            .ki = 0.02f,
            .kd = 0.0f,
            .max_output = 40000.0f,
            .min_output = 0.0f,
            .max_integral = 10000.0f,
            .min_integral = -10000.0f,
            .cal_type = PID_CAL_TYPE_INCREMENTAL,
        }
    };
    pid_new_control_block(&pid_cfg, &pid_handle);
    pid_ctrl_queue_mV = xQueueCreate(4, sizeof(uint32_t));
    xTaskCreate(PID_ctrl_routine, "PID", 1024, NULL, 15, NULL);
    pid_set_voltage(0);
}
