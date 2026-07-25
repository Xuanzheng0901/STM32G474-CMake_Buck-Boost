/**
 * @file    console.c
 * @brief   串口控制台: 运行时参数调谐
 *          命令: V/P <Kp> <Ki> [Kd]  — 电压环 PID
 *                I   <Kp> <Ki>       — 电流环 PI
 *                W   <PF>            — 功率因数 (-1~1)
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
    static float a, b, c;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buf, RX_BUF_SIZE);

    while (1) {
        if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) != pdTRUE)
            continue;

        int n = sscanf((char *)rx_buf, "%c %f %f %f", &cmd, &a, &b, &c);

        switch (cmd) {
        case 'v': case 'V':
            if (n >= 4) {
                ctrl_loop_set_voltage_pi(a, b, c);
                LOGI("console", "电压环 PID: Kp=%.3f Ki=%.3f Kd=%.3f", a, b, c);
            } else {
                LOGE("console", "格式: V <Kp> <Ki> <Kd>");
            }
            break;

        case 'i': case 'I':
            if (n >= 3) {
                ctrl_loop_set_current_pi(a, b);
                LOGI("console", "电流环 PI: Kp=%.3f Ki=%.3f", a, b);
            } else {
                LOGE("console", "格式: I <Kp> <Ki>");
            }
            break;

        case 'w': case 'W':
            if (n >= 2) {
                ctrl_loop_set_pf(a);
                LOGI("console", "PF 已设置: %.3f (%s)",
                     a, (a >= 0) ? "容性/超前" : "感性/滞后");
            } else {
                LOGE("console", "格式: W <PF>  (正=容性, 负=感性)");
            }
            break;

        default:
            LOGE("console", "未知命令: %c (可用: V/I/W)", cmd);
        }

        memset(rx_buf, 0, RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buf, RX_BUF_SIZE);
    }
}

void console_init(void)
{
    xTaskCreate(console_task, "console", 1024, NULL, 5, &task_handle);
}
