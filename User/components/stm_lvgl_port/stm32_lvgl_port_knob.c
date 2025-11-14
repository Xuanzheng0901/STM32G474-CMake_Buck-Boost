/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported to STM32 HAL by Gemini
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h> // For memset
#include "stm32_lvgl_port_knob.h"

#include <stdio.h>

// 调试日志
#define LVGL_PORT_LOGE(format, ...) printf("[LVGL_PORT_E] " format "\r\n", ##__VA_ARGS__)

typedef struct {
    knob_handle_t knob_handle;
    button_handle_t btn_handle;
    lv_indev_t *indev;
    bool btn_enter;
    int32_t diff;
} lvgl_port_encoder_ctx_t;


static void lvgl_port_encoder_read(lv_indev_t *indev_drv, lv_indev_data_t *data);

static void lvgl_port_encoder_btn_down_handler(button_handle_t button_handle, void *usr_data);

static void lvgl_port_encoder_btn_up_handler(button_handle_t button_handle, void *usr_data);

static void lvgl_port_knob_event_handler(void *arg, void *usr_data);

lv_indev_t *lvgl_port_add_encoder(const lvgl_port_encoder_cfg_t *encoder_cfg)
{
    assert(encoder_cfg != NULL);
    assert(encoder_cfg->disp != NULL);

    lvgl_port_encoder_ctx_t *encoder_ctx = malloc(sizeof(lvgl_port_encoder_ctx_t));
    if(encoder_ctx == NULL)
    {
        LVGL_PORT_LOGE("Not enough memory for encoder context allocation!");
        return NULL;
    }
    memset(encoder_ctx, 0, sizeof(lvgl_port_encoder_ctx_t));

    // 创建旋钮
    if(encoder_cfg->encoder_a_b != NULL)
    {
        encoder_ctx->knob_handle = iot_knob_create(encoder_cfg->encoder_a_b);
        if(!encoder_ctx->knob_handle)
        {
            LVGL_PORT_LOGE("Failed to create knob!");
            free(encoder_ctx);
            return NULL;
        }
        iot_knob_register_cb(encoder_ctx->knob_handle, KNOB_LEFT, lvgl_port_knob_event_handler, encoder_ctx);
        iot_knob_register_cb(encoder_ctx->knob_handle, KNOB_RIGHT, lvgl_port_knob_event_handler, encoder_ctx);
    }

    // 注册按键回调
    if(encoder_cfg->encoder_enter != NULL)
    {
        encoder_ctx->btn_handle = encoder_cfg->encoder_enter;
        iot_button_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_DOWN, NULL, lvgl_port_encoder_btn_down_handler,
                               encoder_ctx);
        iot_button_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_UP, NULL, lvgl_port_encoder_btn_up_handler,
                               encoder_ctx);
    }
    encoder_ctx->btn_enter = false;
    encoder_ctx->diff = 0;

    // 注册 LVGL 输入设备
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, lvgl_port_encoder_read);
    lv_indev_set_disp(indev, encoder_cfg->disp);
    lv_indev_set_driver_data(indev, encoder_ctx);
    encoder_ctx->indev = indev;

    return indev;
}

int lvgl_port_remove_encoder(lv_indev_t *encoder)
{
    assert(encoder);
    lvgl_port_encoder_ctx_t *encoder_ctx = (lvgl_port_encoder_ctx_t *)lv_indev_get_driver_data(encoder);

    if(encoder_ctx->knob_handle != NULL)
    {
        iot_knob_delete(encoder_ctx->knob_handle);
    }
    // 注意：按键句柄是外部传入的，这里不删除它
    // if (encoder_ctx->btn_handle != NULL) {
    //     iot_button_delete(encoder_ctx->btn_handle);
    // }

    lv_indev_delete(encoder);

    if(encoder_ctx != NULL)
    {
        free(encoder_ctx);
    }
    return 0;
}

static void lvgl_port_encoder_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    lvgl_port_encoder_ctx_t *ctx = (lvgl_port_encoder_ctx_t *)lv_indev_get_driver_data(indev_drv);
    data->enc_diff = ctx->diff;
    data->state = (ctx->btn_enter == true) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    ctx->diff = 0; // 每次读取后清零
}

static void lvgl_port_encoder_btn_down_handler(button_handle_t button_handle, void *usr_data)
{
    printf("BTN DOWN\n");
    lvgl_port_encoder_ctx_t *ctx = (lvgl_port_encoder_ctx_t *)usr_data;
    ctx->btn_enter = true;
}

static void lvgl_port_encoder_btn_up_handler(button_handle_t button_handle, void *usr_data)
{
    printf("BTN UP\n");
    lvgl_port_encoder_ctx_t *ctx = (lvgl_port_encoder_ctx_t *)usr_data;
    ctx->btn_enter = false;
}

//  ******************************************************************
//  ***  [修正] 使用与原版文件相同的健壮逻辑来处理旋钮事件            ***
//  ******************************************************************
static void lvgl_port_knob_event_handler(void *arg, void *usr_data)
{
    static bool s_is_rotated = false;
    static int8_t direction = -1; // -1 表示无效方向

    lvgl_port_encoder_ctx_t *ctx = (lvgl_port_encoder_ctx_t *)usr_data;
    knob_handle_t knob = (knob_handle_t)arg;
    knob_event_t evt = iot_knob_get_event(knob);
    printf("KNOB ROTATED: %d\r\n", evt);
    // 只关心左右旋转事件
    if(evt != KNOB_LEFT && evt != KNOB_RIGHT)
    {
        return;
    }

    if(!s_is_rotated)
    {
        // 这是物理转动的第一步
        s_is_rotated = true;
        direction = evt;
    }
    else
    {
        // 这是物理转动的第二步
        if(evt == direction)
        {
            // 如果两次方向一致，说明是一次有效的转动，更新 diff
            ctx->diff = (evt == KNOB_LEFT) ? -1 : 1;

            // 在 STM32 端口中, LVGL 任务通常由 osDelay 驱动的循环处理。
            // ctx->diff 的更新会在下一次 lv_task_handler 调用时被 lvgl_port_encoder_read 读取。
            // 不需要像 ESP-IDF 那样显式唤醒任务。
        }
        // 重置状态机，为下一次物理转动做准备
        s_is_rotated = false;
        direction = -1;
    }
}
