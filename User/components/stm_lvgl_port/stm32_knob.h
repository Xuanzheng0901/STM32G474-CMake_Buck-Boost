/*
* SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported to STM32 HAL by Gemini
 */

#ifndef STM32_KNOB_H
#define STM32_KNOB_H

#include <stdint.h>
#include "main.h" // 包含你的 STM32 HAL 库主头文件

#ifdef __cplusplus
extern "C"
{



#endif

typedef void (*knob_cb_t)(void *knob_handle, void *usr_data);

typedef void *knob_handle_t;

// 旋钮事件枚举
typedef enum {
    KNOB_LEFT = 0,
    KNOB_RIGHT,
    KNOB_H_LIM,
    KNOB_L_LIM,
    KNOB_ZERO,
    KNOB_EVENT_MAX,
    KNOB_NONE,
} knob_event_t;

// 旋钮配置
typedef struct {
    uint8_t default_direction; // 0:右旋递增, 1:左旋递增
    GPIO_TypeDef *port_encoder_a;
    uint16_t pin_encoder_a;
    GPIO_TypeDef *port_encoder_b;
    uint16_t pin_encoder_b;
} knob_config_t;

// 创建旋钮
knob_handle_t iot_knob_create(const knob_config_t *config);

// 删除旋钮
int iot_knob_delete(knob_handle_t knob_handle);

// 注册回调
int iot_knob_register_cb(knob_handle_t knob_handle, knob_event_t event, knob_cb_t cb, void *usr_data);

// 注销回调
int iot_knob_unregister_cb(knob_handle_t knob_handle, knob_event_t event);

// 获取事件
knob_event_t iot_knob_get_event(knob_handle_t knob_handle);

// 获取计数值
int iot_knob_get_count_value(knob_handle_t knob_handle);

// 清除计数值
int iot_knob_clear_count_value(knob_handle_t knob_handle);

// 从外部中断恢复（唤醒）扫描定时器
void iot_knob_resume_from_isr(void);

#ifdef __cplusplus
}
#endif

#endif // STM32_KNOB_H
