#include "adc.h"
#include "FreeRTOS.h"
#include "main.h"

#include "console.h"
#include "ctrl_loop.h"
#include "dac.h"
#include "hrtim.h"
#include "lv_port_disp.h"
#include "sogi.h"
#include "task.h"

void LED_task0(void *arg)
{
    LOGI("LED", "Task Running. Stack: %p", xTaskGetCurrentTaskHandle());
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
    }
}

void app_main(void)
{
    log_init(LOG_INFO);
    LOGI("MAIN", "Hello world!");

    xTaskCreate(LED_task0, "LED", 256, NULL, 10, NULL);
    ui_init();
    SOGI_init();
    ADC_init();
    ctrl_loop_init();
    console_init();

    HAL_HRTIM_WaveformCountStart(&hhrtim1,
        HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B);
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
    /* Timer B 初始为安全态 (Boost管关闭) */
    HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);

    HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_MASTER);
    HAL_HRTIM_SimpleBaseStart(&hhrtim1, HRTIM_TIMERINDEX_MASTER);
}
