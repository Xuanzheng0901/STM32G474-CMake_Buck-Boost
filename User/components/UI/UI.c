#include <stdio.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lv_port_encoder.h"
#include "tim.h"
#include "PID.h"

LV_FONT_DECLARE(chillbit);

static lv_indev_t *indev = NULL;
lv_group_t *group = NULL;

static lv_obj_t *voltage_label = NULL;
static lv_obj_t *current_label = NULL;
static lv_obj_t *power_value_label = NULL;
lv_obj_t *voltage_spinbox = NULL;
lv_obj_t *current_spinbox1 = NULL;

static char value_buf[3][32] = {"0.00", "0.00", "00.00W"};

static void lvgl_event_cb(lv_event_t *evt)
{
    int32_t value = lv_spinbox_get_value(lv_event_get_current_target(evt));
    // // ESP_LOGI(TAG, "Value changed: %ld", value);
    if(lv_event_get_current_target(evt) == voltage_spinbox)
    {
        pid_set_voltage(value * 10);
        // snprintf(value_buf[0], 32, "%ld", value);
        // lv_obj_invalidate(voltage_label);
    }
    else if(lv_event_get_current_target(evt) == current_spinbox1)
    {
        pid_set_current_limit(value * 10);
        // snprintf(value_buf[1], 32, "%ld", value);
        // lv_obj_invalidate(current_label);
    }
}

static void value_update_task(void *arg)
{
    while(1)
    {
        vTaskDelay(100);
        float voltage = get_voltage_value(0) / 1000.0f;
        float current = get_voltage_value(1) / 1000.0f;
        snprintf(value_buf[0], 6, "%5.2f", voltage);
        snprintf(value_buf[1], 6, "%5.2f", current);
        snprintf(value_buf[2], 8, "%6.2fW", voltage * current);

        if(lvgl_port_lock(portMAX_DELAY))
        {
            lv_label_set_text_static(voltage_label, value_buf[0]);
            lv_label_set_text_static(current_label, value_buf[1]);
            lv_label_set_text_static(power_value_label, value_buf[2]);
            lvgl_port_unlock();
        }
    }
}


static void indev_init(void)
{
    button_handle_t btn_handle = NULL;
    button_config_t btn_cfg = {0};
    button_gpio_config_t gpio_cfg = {
        .active_level = 0,
        .port         = GPIOB,
        .pin          = GPIO_PIN_5
    };
    iot_button_create_gpio(&btn_cfg, &gpio_cfg, &btn_handle);

    knob_config_t knob_cfg = {
        .default_direction = 1,
        .htim              = &htim2,
    };
    lvgl_port_encoder_cfg_t encoder_cfg = {
        .disp          = lv_display_get_default(),
        .encoder_a_b   = &knob_cfg,
        .encoder_enter = btn_handle
    };
    indev = lvgl_port_add_encoder(&encoder_cfg);
    group = lv_group_create();
    lv_indev_set_group(indev, group);

    LOGI("LVGL", "输入设备初始化完成");
}

static void home_page_init(void)
{
    if(lvgl_port_lock(portMAX_DELAY))
    {
        lv_obj_t *label1 = lv_label_create(lv_screen_active());
        lv_label_set_text(label1, "电压(V)  电流(A)");
        lv_obj_set_width(label1, 128);
        lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(label1, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *label2 = lv_label_create(lv_screen_active());
        lv_label_set_text(label2, "设定");
        lv_obj_set_width(label2, 24);
        lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label2, LV_ALIGN_TOP_LEFT, 0, 16);

        lv_obj_t *label3 = lv_label_create(lv_screen_active());
        lv_label_set_text(label3, "实际");
        lv_obj_set_width(label3, 24);
        lv_obj_set_style_text_align(label3, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label3, LV_ALIGN_TOP_LEFT, 0, 32);

        // 电压数值
        voltage_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(voltage_label, value_buf[0]);
        lv_obj_set_width(voltage_label, 30);
        lv_obj_set_style_text_align(voltage_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(voltage_label, LV_ALIGN_TOP_LEFT, 37, 32);

        //电流数值
        current_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(current_label, value_buf[1]);
        lv_obj_set_width(current_label, 30);
        lv_obj_set_style_text_align(current_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 88, 32);

        lv_obj_t *power_label = lv_label_create(lv_screen_active());
        lv_label_set_text(power_label, "功率");
        lv_obj_set_width(power_label, 42);
        lv_obj_set_style_text_align(power_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(power_label, LV_ALIGN_TOP_LEFT, 0, 48);

        //功率数值
        power_value_label = lv_label_create(lv_screen_active());
        lv_label_set_text(power_value_label, value_buf[2]);
        lv_obj_set_width(power_value_label, 42);
        lv_obj_set_style_text_align(power_value_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(power_value_label, LV_ALIGN_TOP_LEFT, 31, 48);


        //电压调整框
        voltage_spinbox = lv_spinbox_create(lv_screen_active());
        lv_spinbox_set_range(voltage_spinbox, 0, 5000);
        lv_spinbox_set_digit_format(voltage_spinbox, 4, 2);
        lv_spinbox_set_step(voltage_spinbox, 1);

        lv_obj_set_content_height(voltage_spinbox, 12);

        lv_obj_set_style_pad_all(voltage_spinbox, -1, 0);
        lv_obj_set_style_border_width(voltage_spinbox, 1, 0);
        lv_obj_set_style_border_side(voltage_spinbox, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_opa(voltage_spinbox, 0, 0);
        lv_obj_set_style_radius(voltage_spinbox, 0, 0);

        // 聚焦时显示下划线
        lv_obj_set_style_border_opa(voltage_spinbox, LV_OPA_COVER, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_width(voltage_spinbox, 1, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_color(voltage_spinbox, lv_color_black(), LV_STATE_FOCUS_KEY);

        lv_obj_set_size(voltage_spinbox, 32, 12);

        lv_obj_set_style_outline_opa(voltage_spinbox, 0, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_opa(voltage_spinbox, 0, LV_STATE_EDITED);

        lv_obj_set_style_bg_color(voltage_spinbox, lv_color_black(), LV_PART_CURSOR | LV_STATE_EDITED);
        lv_obj_set_style_text_color(voltage_spinbox, lv_color_white(), LV_PART_CURSOR | LV_STATE_EDITED);
        lv_obj_set_style_bg_color(voltage_spinbox, lv_color_white(), LV_PART_CURSOR);
        lv_obj_set_style_text_color(voltage_spinbox, lv_color_black(), LV_PART_CURSOR);

        lv_obj_set_style_y(voltage_spinbox, 12, LV_PART_CURSOR | LV_STATE_EDITED);

        lv_obj_set_style_text_align(voltage_spinbox, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_align(voltage_spinbox, LV_ALIGN_TOP_LEFT, 36, 18);
        lv_group_add_obj(group, voltage_spinbox);
        lv_obj_add_event_cb(voltage_spinbox, lvgl_event_cb, LV_EVENT_VALUE_CHANGED, NULL);


        //电流调整框
        current_spinbox1 = lv_spinbox_create(lv_screen_active());
        lv_spinbox_set_range(current_spinbox1, 0, 600);
        lv_spinbox_set_digit_format(current_spinbox1, 3, 1);
        lv_spinbox_set_step(current_spinbox1, 1);
        lv_spinbox_set_value(current_spinbox1, 100);

        lv_obj_set_content_height(current_spinbox1, 12);

        lv_obj_set_style_pad_all(current_spinbox1, -1, 0);
        lv_obj_set_style_border_width(current_spinbox1, 1, 0);
        lv_obj_set_style_border_side(current_spinbox1, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_opa(current_spinbox1, 0, 0);
        lv_obj_set_style_radius(current_spinbox1, 0, 0);

        // 聚焦时显示下划线
        lv_obj_set_style_border_opa(current_spinbox1, LV_OPA_COVER, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_width(current_spinbox1, 1, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_color(current_spinbox1, lv_color_black(), LV_STATE_FOCUS_KEY);

        lv_obj_set_size(current_spinbox1, 26, 12);

        lv_obj_set_style_outline_opa(current_spinbox1, 0, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_opa(current_spinbox1, 0, LV_STATE_EDITED);

        lv_obj_set_style_bg_color(current_spinbox1, lv_color_black(), LV_PART_CURSOR | LV_STATE_EDITED);
        lv_obj_set_style_text_color(current_spinbox1, lv_color_white(), LV_PART_CURSOR | LV_STATE_EDITED);
        lv_obj_set_style_bg_color(current_spinbox1, lv_color_white(), LV_PART_CURSOR);
        lv_obj_set_style_text_color(current_spinbox1, lv_color_black(), LV_PART_CURSOR);

        lv_obj_set_style_text_align(current_spinbox1, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_align(current_spinbox1, LV_ALIGN_TOP_RIGHT, -9, 18);
        lv_group_add_obj(group, current_spinbox1);
        lv_obj_add_event_cb(current_spinbox1, lvgl_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t *my_label = lv_label_create(lv_scr_act());
        lv_label_set_text(my_label, "电压电流");
        lv_obj_set_style_text_font(my_label, &chillbit, 0);
        lv_obj_align(my_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_width(my_label, 128);
        lvgl_port_unlock();
    }
}

void ui_init(void)
{
    lv_port_disp_init();
    indev_init();
    home_page_init();
    LOGI("LVGL", "界面初始化完成");
    LOGI("LVGL", "Hello LVGL!");
    if(xTaskCreate(value_update_task, "update value", 384, NULL, 10, NULL) != pdPASS)
    {
        printf("update value task creation failed\n");
    }
}
