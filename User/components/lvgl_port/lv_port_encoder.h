/*
* SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported to STM32 HAL by Gemini
 */
#ifndef STM32_LVGL_PORT_KNOB_H
#define STM32_LVGL_PORT_KNOB_H

#include "lvgl.h"
#include "lv_port_knob.h"
#include "lv_port_button.h"

#ifdef __cplusplus
extern "C"
{
#endif

// LVGL 旋钮和按键的配置
typedef struct {
    lv_disp_t *disp; // LVGL 显示设备句柄
    const knob_config_t *encoder_a_b; // 旋钮配置
    button_handle_t encoder_enter; // 按键句柄 (外部创建并传入)
} lvgl_port_encoder_cfg_t;


// 添加编码器作为 LVGL 输入设备
lv_indev_t *lvgl_port_add_encoder(const lvgl_port_encoder_cfg_t *encoder_cfg);

// 移除编码器
int lvgl_port_remove_encoder(lv_indev_t * encoder);


#ifdef __cplusplus
}
#endif

#endif // STM32_LVGL_PORT_KNOB_H