#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "usart.h"
#include "LOG.h"
#include <stdio.h>
#include <string.h>
#include "ctrl_loop.h"

#define CONSOLE_RX_BUF_SIZE 64
static uint8_t console_rx_buf[CONSOLE_RX_BUF_SIZE] = {0};
static TaskHandle_t consoleTaskHandle;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart->Instance == USART3)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(consoleTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void console_task(void *pvParameters)
{
    static char index = 0;
    static float kp = 0.0f, ki = 0.0f, kd = 0.0f;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, console_rx_buf, CONSOLE_RX_BUF_SIZE);

    while(1)
    {
        uint32_t ulNotificationValue;
        if(xTaskNotifyWait(0, portMAX_DELAY, &ulNotificationValue, portMAX_DELAY) == pdTRUE)
        {
            if(sscanf((char *)console_rx_buf, "%c %f %f %f", &index, &kp, &ki, &kd) == 4)
            {
                switch(index)
                {
                    case 'v':
                    case 'V':
                        ctrl_loop_set_voltage_pi(kp, ki, kd);
                        LOGI("console", "应用成功");
                        break;
                    case 'i':
                    case 'I':
                        ctrl_loop_set_current_pi(kp, ki);
                        LOGI("console", "应用成功");
                        break;
                    default:
                        LOGE("console", "前缀不正确");
                }
            }
            else
            {
                LOGI("CONSOLE", "Parse error. Input: %s", console_rx_buf);
            }
            memset(console_rx_buf, 0, CONSOLE_RX_BUF_SIZE);
            HAL_UARTEx_ReceiveToIdle_DMA(&huart3, console_rx_buf, CONSOLE_RX_BUF_SIZE);
        }
    }
}

void console_init(void)
{
    xTaskCreate(console_task, "console_task", 1024, NULL, 5, &consoleTaskHandle);
}
