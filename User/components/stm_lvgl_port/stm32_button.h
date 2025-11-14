/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported to STM32 HAL by Gemini
 */

#ifndef STM32_BUTTON_H
#define STM32_BUTTON_H

#include <stdint.h>
#include <stddef.h>
#include "main.h" // 包含你的 STM32 HAL 库主头文件

#ifdef __cplusplus
extern "C"
{



#endif

// 按钮状态定义
enum {
    BUTTON_INACTIVE = 0,
    BUTTON_ACTIVE,
};

// 按钮句柄类型
typedef struct button_dev_t *button_handle_t;

// 按钮回调函数类型
typedef void (*button_cb_t)(button_handle_t btn_handle, void *usr_data);

// 按钮事件枚举
typedef enum {
    BUTTON_PRESS_DOWN = 0,
    BUTTON_PRESS_UP,
    BUTTON_PRESS_REPEAT,
    BUTTON_PRESS_REPEAT_DONE,
    BUTTON_SINGLE_CLICK,
    BUTTON_DOUBLE_CLICK,
    BUTTON_MULTIPLE_CLICK,
    BUTTON_LONG_PRESS_START,
    BUTTON_LONG_PRESS_HOLD,
    BUTTON_LONG_PRESS_UP,
    BUTTON_PRESS_END,
    BUTTON_EVENT_MAX,
    BUTTON_NONE_PRESS,
} button_event_t;


// 按钮事件参数
typedef union {
    struct long_press_t {
        uint16_t press_time;
    } long_press;

    struct multiple_clicks_t {
        uint16_t clicks;
    } multiple_clicks;
} button_event_args_t;


// 按钮参数类型
typedef enum {
    BUTTON_LONG_PRESS_TIME_MS = 0,
    BUTTON_SHORT_PRESS_TIME_MS,
} button_param_t;


// 按钮常规配置
typedef struct {
    uint16_t long_press_time;
    uint16_t short_press_time;
} button_config_t;

// GPIO 按钮特定配置
typedef struct {
    GPIO_TypeDef *port; // GPIO 端口
    uint16_t pin; // GPIO 引脚
    uint8_t active_level; // 按下时的有效电平 (0 或 1)
} button_gpio_config_t;


// 创建一个新的 GPIO 按钮设备
int iot_button_create_gpio(const button_config_t *button_config, const button_gpio_config_t *gpio_cfg,
                           button_handle_t *ret_button);

// 删除按钮
int iot_button_delete(button_handle_t btn_handle);

// 注册按钮事件回调
int iot_button_register_cb(button_handle_t btn_handle, button_event_t event, button_event_args_t *event_args,
                           button_cb_t cb, void *usr_data);

// 注销按钮事件回调
int iot_button_unregister_cb(button_handle_t btn_handle, button_event_t event, button_event_args_t *event_args);

// 获取按钮事件
button_event_t iot_button_get_event(button_handle_t btn_handle);

// 获取按钮重复次数
uint8_t iot_button_get_repeat(button_handle_t btn_handle);

// 获取按钮按下持续时间
uint32_t iot_button_get_ticks_time(button_handle_t btn_handle);

// 获取长按保持计数
uint16_t iot_button_get_long_press_hold_cnt(button_handle_t btn_handle);

// 设置参数
int iot_button_set_param(button_handle_t btn_handle, button_param_t param, void *value);

// 获取按键电平
uint8_t iot_button_get_key_level(button_handle_t btn_handle);

// 从外部中断恢复（唤醒）扫描定时器
void iot_button_resume_from_isr(void);

#ifdef __cplusplus
}
#endif

#endif // STM32_BUTTON_H
