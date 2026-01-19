#include <stdio.h>
#include <stdlib.h>
#include <math.h> // 需要 abs()
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "lv_port_knob.h"
#include <stdbool.h>

#define CONFIG_KNOB_PERIOD_TIME_MS      (15)
#define KNOB_ECODER_RESOLUTION          (2) // 2次脉冲计数
#define CONFIG_KNOB_HIGH_LIMIT          (2147483647) // 上限
#define CONFIG_KNOB_LOW_LIMIT           (-2147483647) //下限

#define TICKS_INTERVAL    CONFIG_KNOB_PERIOD_TIME_MS
#define HIGH_LIMIT        CONFIG_KNOB_HIGH_LIMIT
#define LOW_LIMIT         CONFIG_KNOB_LOW_LIMIT

typedef struct Knob {
    TIM_HandleTypeDef *htim;
    uint32_t last_hw_counter;
    uint8_t default_direction;
    knob_event_t event;
    int32_t count_value;
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
    if(!config || !config->htim)
        return NULL;

    knob_dev_t *knob = (knob_dev_t *)calloc(1, sizeof(knob_dev_t));
    if(!knob)
        return NULL;

    knob->htim = config->htim;
    knob->default_direction = config->default_direction;

    HAL_TIM_Encoder_Start(knob->htim, TIM_CHANNEL_ALL);

    knob->last_hw_counter = (uint32_t)__HAL_TIM_GET_COUNTER(knob->htim);

    knob->event = KNOB_NONE;
    knob->count_value = 0;

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
            free(knob);
            return NULL;
        }
    }

    return knob;
}

int32_t iot_knob_delete(knob_handle_t knob_handle)
{
    if(!knob_handle)
        return -1;
    knob_dev_t *knob = knob_handle;

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
    HAL_TIM_Encoder_Stop(knob->htim, TIM_CHANNEL_ALL);
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

static void knob_handler(knob_dev_t *knob)
{
    uint32_t current_hw_counter = (uint32_t)__HAL_TIM_GET_COUNTER(knob->htim);
    int32_t diff = (int32_t)(current_hw_counter - knob->last_hw_counter);

    if(abs(diff) >= KNOB_ECODER_RESOLUTION)
    {
        int32_t steps = diff / KNOB_ECODER_RESOLUTION;

        knob->last_hw_counter += (steps * KNOB_ECODER_RESOLUTION);

        if(steps > 0) //正转
        {
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
        }
        else //反转
        {
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
        }

        if(knob->count_value >= HIGH_LIMIT)
        {
            knob->count_value = HIGH_LIMIT;
            knob->event = KNOB_H_LIM;
            CALL_EVENT_CB(KNOB_H_LIM);
        }
        else if(knob->count_value <= LOW_LIMIT)
        {
            knob->count_value = LOW_LIMIT;
            knob->event = KNOB_L_LIM;
            CALL_EVENT_CB(KNOB_L_LIM);
        }
        else if(knob->count_value == 0)
        {
            knob->event = KNOB_ZERO;
            CALL_EVENT_CB(KNOB_ZERO);
        }
    }
}

int32_t iot_knob_register_cb(knob_handle_t knob_handle, knob_event_t event, knob_cb_t cb, void *usr_data)
{
    if(!knob_handle || event >= KNOB_EVENT_MAX)
        return -1;
    knob_dev_t *knob = knob_handle;
    knob->cb[event] = cb;
    knob->usr_data[event] = usr_data;
    return 0;
}

int32_t iot_knob_unregister_cb(knob_handle_t knob_handle, knob_event_t event)
{
    if(!knob_handle || event >= KNOB_EVENT_MAX)
        return -1;
    knob_dev_t *knob = knob_handle;
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

int32_t iot_knob_get_count_value(knob_handle_t knob_handle)
{
    if(!knob_handle)
        return 0;
    return ((knob_dev_t *)knob_handle)->count_value;
}

int32_t iot_knob_clear_count_value(knob_handle_t knob_handle)
{
    if(!knob_handle)
        return -1;
    ((knob_dev_t *)knob_handle)->count_value = 0;
    return 0;
}
