#include "pid_ctrl.h"
#include "FreeRTOS.h"
#include "hrtim.h"
#include "main.h"
#include "task.h"
#include "queue.h"

extern QueueHandle_t adc_queue; // dma中断传输指针用队列

static pid_ctrl_block_handle_t pid_handle = NULL;
QueueHandle_t pid_ctrl_queue_mV = NULL; //传递数值单位为mV

static uint32_t voltage_to_pwm_duty(float voltage_mV)
{
    if(voltage_mV < 10.0f)
    {
        return 0;
    }
    uint32_t pwm_duty = (uint32_t)(1 / (1 + (30000.0f / voltage_mV)) * PWM_Period); //预设输入电压为30V
    if(pwm_duty > PWM_Period * 0.6f)
        pwm_duty = PWM_Period;
    return pwm_duty;
}

void pwm_set_duty(uint32_t pwm_duty)
{
    hhrtim1.Instance->sTimerxRegs[4].CMP1CxR = pwm_duty;
}

static void PID_ctrl_routine(void *pvParameters)
{
    static uint16_t last_duty = 0;
    static uint32_t target_voltage_buffer_mV = 0;
    static uint32_t target_voltage_mV = 0;
    static float pid_output_mV = 0.0f;

    static uint32_t *buf_ptr;

    while(1)
    {
        if(xQueueReceive(adc_queue, &buf_ptr, portMAX_DELAY) == pdTRUE)
        {
            do
            {
                if(pdPASS == xQueueReceive(pid_ctrl_queue_mV, &target_voltage_buffer_mV, 0))
                {
                    if(target_voltage_mV == target_voltage_buffer_mV)
                        break;
                    target_voltage_mV = target_voltage_buffer_mV;
                    // 目标改变时重置PID状态
                    pwm_set_duty(last_duty);
                    pid_reset_ctrl_block(pid_handle);
                    break;
                }
            }
            while(0); // 先检查目标电压有无更改

            // 计算ADC平均值
            float voltage_origin_summary = 0.0f;
            float current_origin_summary = 0.0f;
            for(uint8_t i = 0; i < ADC_BUFFER_LENGTH / 2; i++)
            {
                voltage_origin_summary += (uint16_t)buf_ptr[i];
                current_origin_summary += buf_ptr[i] >> 16;
            }
            voltage_origin_summary = voltage_origin_summary / 25.0f * 3300.0f / 4095.0f * 20.0f; //单位mV
            current_origin_summary = current_origin_summary / 25.0f * 3300.0f / 4095.0f; //单位mV

            // 计算误差: error = 期望值 - 实际值
            float err = (float)target_voltage_mV - voltage_origin_summary;
            // PID计算 (位置式PID，输出为绝对控制量)
            pid_compute(pid_handle, err, &pid_output_mV);

            // 非线性补偿: 将PID输出的电压值映射到PWM占空比
            uint16_t pwm_duty = voltage_to_pwm_duty(pid_output_mV);

            // 更新PWM输出
            pwm_set_duty(pwm_duty);
            last_duty = pwm_duty;
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
            .kp = 0.01f,
            .ki = 0.05f,
            .kd = 0.02f,
            .max_output = 80000.0f,
            .min_output = 0.0f,
            .max_integral = 300.0f,
            .min_integral = -300.0f,
            .cal_type = PID_CAL_TYPE_POSITIONAL,
        }
    };
    pid_new_control_block(&pid_cfg, &pid_handle);
    pid_ctrl_queue_mV = xQueueCreate(4, sizeof(uint32_t));
    xTaskCreate(PID_ctrl_routine, "PID", 1024, NULL, 15, NULL);
    pid_set_voltage(0);
}
