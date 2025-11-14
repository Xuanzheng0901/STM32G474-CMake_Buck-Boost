//
// Created by Pluviophile on 2025/10/29.
/*
 * SPDX-FileCopyrightText: 2016-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported to STM32 HAL by Gemini
 */

#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stm32_knob.h"

#include <stdbool.h>

// 调试日志
// #define KNOB_LOGE(format, ...) printf("[KNOB_E] " format "\r\n", ##__VA_ARGS__)
// #define KNOB_LOGI(format, ...) printf("[KNOB_I] " format "\r\n", ##__VA_ARGS__)

// 配置参数
#ifndef CONFIG_KNOB_PERIOD_TIME_MS
#define CONFIG_KNOB_PERIOD_TIME_MS      (2)   // 扫描周期 (ms)
#endif

#ifndef CONFIG_KNOB_DEBOUNCE_TICKS
#define CONFIG_KNOB_DEBOUNCE_TICKS      (0)   // 消抖次数 (2 * 5ms = 10ms)
#endif

#ifndef CONFIG_KNOB_HIGH_LIMIT
#define CONFIG_KNOB_HIGH_LIMIT          (100)
#endif

#ifndef CONFIG_KNOB_LOW_LIMIT
#define CONFIG_KNOB_LOW_LIMIT           (-100)
#endif

// 内部常量
#define TICKS_INTERVAL    CONFIG_KNOB_PERIOD_TIME_MS
#define DEBOUNCE_TICKS    CONFIG_KNOB_DEBOUNCE_TICKS
#define HIGH_LIMIT        CONFIG_KNOB_HIGH_LIMIT
#define LOW_LIMIT         CONFIG_KNOB_LOW_LIMIT

typedef enum {
    KNOB_STATE_CHECK = -1,
    KNOB_STATE_READY = 0,
    KNOB_STATE_PHASE_A,
    KNOB_STATE_PHASE_B,
} knob_state_t;

typedef struct Knob {
    GPIO_TypeDef *port_a;
    uint16_t pin_a;
    GPIO_TypeDef *port_b;
    uint16_t pin_b;

    bool encoder_a_change;
    bool encoder_b_change;
    uint8_t default_direction;
    knob_state_t state;
    uint8_t debounce_a_cnt;
    uint8_t debounce_b_cnt;
    uint8_t encoder_a_level;
    uint8_t encoder_b_level;
    knob_event_t event;
    uint16_t ticks;
    int count_value;

    void *usr_data[KNOB_EVENT_MAX];
    knob_cb_t cb[KNOB_EVENT_MAX];
    struct Knob *next;
} knob_dev_t;

static knob_dev_t *s_head_handle = NULL;
static TimerHandle_t s_knob_timer_handle = NULL;

static void knob_scan_cb(TimerHandle_t xTimer);

static void knob_handler(knob_dev_t *knob);

#define CALL_EVENT_CB(ev)   if(knob->cb[ev]) knob->cb[ev](knob, knob->usr_data[ev])

knob_handle_t iot_knob_create(const knob_config_t *config)
{
    if(!config)
    {
        // KNOB_LOGE("config pointer is NULL!");
        return NULL;
    }

    knob_dev_t *knob = (knob_dev_t *)calloc(1, sizeof(knob_dev_t));
    if(!knob)
    {
        // KNOB_LOGE("alloc knob failed");
        return NULL;
    }

    knob->port_a = config->port_encoder_a;
    knob->pin_a = config->pin_encoder_a;
    knob->port_b = config->port_encoder_b;
    knob->pin_b = config->pin_encoder_b;
    knob->default_direction = config->default_direction;

    knob->encoder_a_level = HAL_GPIO_ReadPin(knob->port_a, knob->pin_a);
    knob->encoder_b_level = HAL_GPIO_ReadPin(knob->port_b, knob->pin_b);

    knob->state = KNOB_STATE_CHECK;
    knob->event = KNOB_NONE;

    knob->next = s_head_handle;
    s_head_handle = knob;

    if(!s_knob_timer_handle)
    {
        s_knob_timer_handle = xTimerCreate("knob_timer",
                                           pdMS_TO_TICKS(TICKS_INTERVAL),
                                           pdTRUE,
                                           NULL,
                                           knob_scan_cb);
        if(s_knob_timer_handle)
        {
            xTimerStart(s_knob_timer_handle, 0);
        }
        else
        {
            // KNOB_LOGE("Failed to create knob timer");
            free(knob);
            return NULL;
        }
    }

    // KNOB_LOGI("IoT Knob for STM32 Created. A:(%p, %d), B:(%p, %d)",
    //           knob->port_a, knob->pin_a, knob->port_b, knob->pin_b);

    return (knob_handle_t)knob;
}

int iot_knob_delete(knob_handle_t knob_handle)
{
    if(!knob_handle)
        return -1;
    knob_dev_t *knob = (knob_dev_t *)knob_handle;

    for(knob_dev_t **curr = &s_head_handle; *curr;)
    {
        knob_dev_t *entry = *curr;
        if(entry == knob)
        {
            *curr = entry->next;
            free(entry);
        }
        else
        {
            curr = &entry->next;
        }
    }

    if(s_head_handle == NULL && s_knob_timer_handle)
    {
        xTimerStop(s_knob_timer_handle, 0);
        xTimerDelete(s_knob_timer_handle, 0);
        s_knob_timer_handle = NULL;
    }
    return 0;
}

static void knob_scan_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    for(knob_dev_t *target = s_head_handle; target; target = target->next)
    {
        knob_handler(target);
    }
}


void iot_knob_resume_from_isr(void)
{
    if(s_knob_timer_handle)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerStartFromISR(s_knob_timer_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void knob_handler(knob_dev_t *knob)
{
    uint8_t pha_value = HAL_GPIO_ReadPin(knob->port_a, knob->pin_a);
    uint8_t phb_value = HAL_GPIO_ReadPin(knob->port_b, knob->pin_b);

    if((knob->state) > 0)
    {
        knob->ticks++;
    }

    if(pha_value != knob->encoder_a_level)
    {
        if(++(knob->debounce_a_cnt) >= DEBOUNCE_TICKS)
        {
            knob->encoder_a_change = true;
            knob->encoder_a_level = pha_value;
            knob->debounce_a_cnt = 0;
        }
    }
    else
    {
        knob->debounce_a_cnt = 0;
    }

    if(phb_value != knob->encoder_b_level)
    {
        if(++(knob->debounce_b_cnt) >= DEBOUNCE_TICKS)
        {
            knob->encoder_b_change = true;
            knob->encoder_b_level = phb_value;
            knob->debounce_b_cnt = 0;
        }
    }
    else
    {
        knob->debounce_b_cnt = 0;
    }

    switch(knob->state)
    {
        case KNOB_STATE_READY:
            if(knob->encoder_a_change)
            {
                knob->encoder_a_change = false;
                knob->ticks = 0;
                knob->state = KNOB_STATE_PHASE_A;
            }
            else if(knob->encoder_b_change)
            {
                knob->encoder_b_change = false;
                knob->ticks = 0;
                knob->state = KNOB_STATE_PHASE_B;
            }
            else
            {
                knob->event = KNOB_NONE;
            }
            break;

        case KNOB_STATE_PHASE_A:
            if(knob->encoder_b_change)
            {
                knob->encoder_b_change = false;
                if(knob->default_direction)
                {
                    knob->count_value--;
                    knob->event = KNOB_LEFT;
                    CALL_EVENT_CB(KNOB_LEFT);
                }
                else
                {
                    knob->count_value++;
                    knob->event = KNOB_RIGHT;
                    CALL_EVENT_CB(KNOB_RIGHT);
                }
                // Check limits after update
                if(knob->count_value >= HIGH_LIMIT)
                {
                    knob->event = KNOB_H_LIM;
                    CALL_EVENT_CB(KNOB_H_LIM);
                }
                else if(knob->count_value <= LOW_LIMIT)
                {
                    knob->event = KNOB_L_LIM;
                    CALL_EVENT_CB(KNOB_L_LIM);
                }
                else if(knob->count_value == 0)
                {
                    knob->event = KNOB_ZERO;
                    CALL_EVENT_CB(KNOB_ZERO);
                }
                knob->ticks = 0;
                knob->state = KNOB_STATE_READY;
            }
            else if(knob->encoder_a_change)
            { // Glitch
                knob->encoder_a_change = false;
                knob->ticks = 0;
                knob->state = KNOB_STATE_READY;
            }
            break;

        case KNOB_STATE_PHASE_B:
            if(knob->encoder_a_change)
            {
                knob->encoder_a_change = false;
                if(knob->default_direction)
                {
                    knob->count_value++;
                    knob->event = KNOB_RIGHT;
                    CALL_EVENT_CB(KNOB_RIGHT);
                }
                else
                {
                    knob->count_value--;
                    knob->event = KNOB_LEFT;
                    CALL_EVENT_CB(KNOB_LEFT);
                }
                // Check limits after update
                if(knob->count_value >= HIGH_LIMIT)
                {
                    knob->event = KNOB_H_LIM;
                    CALL_EVENT_CB(KNOB_H_LIM);
                }
                else if(knob->count_value <= LOW_LIMIT)
                {
                    knob->event = KNOB_L_LIM;
                    CALL_EVENT_CB(KNOB_L_LIM);
                }
                else if(knob->count_value == 0)
                {
                    knob->event = KNOB_ZERO;
                    CALL_EVENT_CB(KNOB_ZERO);
                }
                knob->ticks = 0;
                knob->state = KNOB_STATE_READY;
            }
            else if(knob->encoder_b_change)
            { // Glitch
                knob->encoder_b_change = false;
                knob->ticks = 0;
                knob->state = KNOB_STATE_READY;
            }
            break;

        case KNOB_STATE_CHECK:
            if(knob->encoder_a_level == 1 && knob->encoder_b_level == 1)
            {
                knob->state = KNOB_STATE_READY;
                knob->encoder_a_change = false;
                knob->encoder_b_change = false;
            }
            break;
    }
}


int iot_knob_register_cb(knob_handle_t knob_handle, knob_event_t event, knob_cb_t cb, void *usr_data)
{
    if(!knob_handle || event >= KNOB_EVENT_MAX)
        return -1;
    knob_dev_t *knob = (knob_dev_t *)knob_handle;
    knob->cb[event] = cb;
    knob->usr_data[event] = usr_data;
    return 0;
}

int iot_knob_unregister_cb(knob_handle_t knob_handle, knob_event_t event)
{
    if(!knob_handle || event >= KNOB_EVENT_MAX)
        return -1;
    knob_dev_t *knob = (knob_dev_t *)knob_handle;
    knob->cb[event] = NULL;
    knob->usr_data[event] = NULL;
    return 0;
}

knob_event_t iot_knob_get_event(knob_handle_t knob_handle)
{
    if(!knob_handle)
        return KNOB_NONE;
    return ((knob_dev_t *)knob_handle)->event;
}

int iot_knob_get_count_value(knob_handle_t knob_handle)
{
    if(!knob_handle)
        return 0;
    return ((knob_dev_t *)knob_handle)->count_value;
}

int iot_knob_clear_count_value(knob_handle_t knob_handle)
{
    if(!knob_handle)
        return -1;
    ((knob_dev_t *)knob_handle)->count_value = 0;
    return 0;
}
