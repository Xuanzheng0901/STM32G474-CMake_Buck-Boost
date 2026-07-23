#include "adc.h"
#include "FreeRTOS.h"
#include "main.h"

#include <math.h>

#include "ctrl_loop.h"
#include "dac.h"
#include "task.h"
#include "stdio_ext.h"
#include "string.h"
#include "hrtim.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "sogi.h"
#include "spi.h"

void LED_task0(void *arg)
{
    LOGI("LED", "Task Running. Stack: %p", xTaskGetCurrentTaskHandle());
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
    }
}

void app_main(void)
{
    log_init(LOG_INFO);
    LOGI("MAIN", "Hello world!");

    // printf("helloworld\n");
    xTaskCreate(LED_task0, "LED", 256, NULL, 10, NULL);
    ui_init();
    SOGI_init();
    ADC_init();         /* 先创建 adc_queue, 再启动控制任务 */
    ctrl_loop_init();

    HAL_HRTIM_WaveformCountStart(
        &hhrtim1,
        HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B | HRTIM_TIMERID_TIMER_C);
    HAL_HRTIM_WaveformOutputStart(&hhrtim1,HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
    HAL_HRTIM_WaveformOutputStart(&hhrtim1,HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
    HAL_HRTIM_WaveformOutputStart(&hhrtim1,HRTIM_OUTPUT_TC1 | HRTIM_OUTPUT_TC2);

    HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_MASTER);
    HAL_HRTIM_SimpleBaseStart_IT(&hhrtim1, HRTIM_TIMERINDEX_MASTER);
}
