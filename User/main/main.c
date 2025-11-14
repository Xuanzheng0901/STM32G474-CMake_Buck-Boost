#include "adc.h"
#include "FreeRTOS.h"
#include "main.h"
#include "queue.h"
#include "task.h"
#include "usart.h"
#include "stdio_ext.h"
#include "string.h"
#include "../components/FATFS/app_flash.h"
#include "hrtim.h"
#include "lvgl.h"
#include "../components/stm_lvgl_port/lv_port_disp_template.h"

extern TaskHandle_t adc_task_handle;

void LED_task0(void *arg)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
    }
}

void app_main(void)
{
    lv_port_disp_init();
    home_page_init();
    xTaskCreate(ADC_read_task, "ADC", 1024, NULL, 50, &adc_task_handle);
    xTaskCreate(LED_task0, "LED", 128, NULL, 10, NULL);
    HAL_HRTIM_WaveformCounterStart(&hhrtim1, HRTIM_TIMERID_TIMER_E);
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TE1 | HRTIM_OUTPUT_TE2);
    hhrtim1.Instance->sTimerxRegs[4].CMP1CxR = 0;
}
