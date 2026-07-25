/**
 * @file    console.c
 * @brief   串口控制台: 运行时 PID 参数调谐
 *          格式: <V|I> <Kp> <Ki> [Kd]
 */

#include "console.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "usart.h"
#include "LOG.h"
#include "ctrl_loop.h"
#include <stdio.h>
#include <string.h>

#define RX_BUF_SIZE 64

static uint8_t rx_buf[RX_BUF_SIZE];
static TaskHandle_t task_handle;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3) {
        BaseType_t woke = pdFALSE;
        vTaskNotifyGiveFromISR(task_handle, &woke);
        portYIELD_FROM_ISR(woke);
    }
}

static void console_task(void *pvParameters)
{
    static char cmd;
    static float kp, ki, kd;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buf, RX_BUF_SIZE);

    while (1) {
        if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) != pdTRUE)
            continue;

        if (sscanf((char *)rx_buf, "%c %f %f %f", &cmd, &kp, &ki, &kd) >= 3) {
            switch (cmd) {
            case 'v': case 'V':
                ctrl_loop_set_voltage_pi(kp, ki, kd);
                LOGI("console", "电压环 PID 已更新: Kp=%.3f Ki=%.3f Kd=%.3f", kp, ki, kd);
                break;
            case 'i': case 'I':
                ctrl_loop_set_current_pi(kp, ki);
                LOGI("console", "电流环 PI 已更新: Kp=%.3f Ki=%.3f", kp, ki);
                break;
            default:
                LOGE("console", "未知命令: %c (可用: V/I)", cmd);
            }
        } else {
            LOGI("CONSOLE", "解析失败: %s", rx_buf);
        }
        memset(rx_buf, 0, RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buf, RX_BUF_SIZE);
    }
}

void console_init(void)
{
    xTaskCreate(console_task, "console", 1024, NULL, 5, &task_handle);
}
