#include "pid_ctrl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

static pid_ctrl_block_handle_t pid_handle = NULL;
QueueHandle_t pid_ctrl_queue_mV = NULL; //传递数值单位为mV

void PID_ctrl_routine(void *pvParameters)
{
    static uint16_t last_duty = 0;
    static uint16_t target_voltage_mV = 0;
    static float pid_calc_result_mV = 0;

    while(1)
    {}
}
