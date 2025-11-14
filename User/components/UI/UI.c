#include <stdio.h>

#include "lvgl.h"
#include "../stm_lvgl_port/lv_port_disp_template.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../stm_lvgl_port/stm32_lvgl_port_knob.h"

extern lv_obj_t *highlight_frame;
extern lv_anim_t focus_anim;

extern void focus_event_cb(lv_event_t *e);

// static lv_display_t *disp = NULL;
static lv_indev_t *indev = NULL;
lv_group_t *group = NULL;

static lv_obj_t *voltage_label = NULL;
static lv_obj_t *current_label = NULL;
static lv_obj_t *power_value_label = NULL;
lv_obj_t *voltage_spinbox = NULL;
lv_obj_t *current_spinbox = NULL;

static char value_buf[3][32] = {"0.00", "0.00", "00.00W"};

static void lvgl_event_cb(lv_event_t *evt)
{
    int32_t value = lv_spinbox_get_value(lv_event_get_current_target(evt));
    // // ESP_LOGI(TAG, "Value changed: %ld", value);
    if(lv_event_get_current_target(evt) == voltage_spinbox)
    {
        // pid_set_voltage(value * 10);
        snprintf(value_buf[0], 32, "%ld", value);
        lv_obj_invalidate(voltage_label);
    }
    else
    {
        // set_current(value * 10);
        snprintf(value_buf[1], 32, "%ld", value);
        lv_obj_invalidate(current_label);
    }
}


void indev_init(void)
{
    button_handle_t btn_handle = NULL;
    button_config_t btn_cfg = {0};
    button_gpio_config_t gpio_cfg = {
        .active_level = 0,
        .port = GPIOB,
        .pin = GPIO_PIN_3
    };
    iot_button_create_gpio(&btn_cfg, &gpio_cfg, &btn_handle);

    knob_config_t knob_cfg = {
        .default_direction = 0,
        .port_encoder_a = GPIOB,
        .pin_encoder_a = GPIO_PIN_4,
        .port_encoder_b = GPIOB,
        .pin_encoder_b = GPIO_PIN_5
    };
    lvgl_port_encoder_cfg_t encoder_cfg = {
        .disp = lv_display_get_default(),
        .encoder_a_b = &knob_cfg,
        .encoder_enter = btn_handle
    };
    indev = lvgl_port_add_encoder(&encoder_cfg);
    group = lv_group_create();
    lv_indev_set_group(indev, group);
}

void home_page_init(void)
{
    indev_init();
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
        lv_obj_set_width(current_label, 24);
        lv_obj_set_style_text_align(current_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 94, 32);

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
        lv_spinbox_set_range(voltage_spinbox, 200, 2500);
        lv_spinbox_set_digit_format(voltage_spinbox, 4, 2);
        lv_spinbox_set_step(voltage_spinbox, 1);

        lv_obj_set_content_height(voltage_spinbox, 12);

        lv_obj_set_style_pad_all(voltage_spinbox, -1, 0);
        lv_obj_set_style_border_width(voltage_spinbox, 0, 0);

        lv_obj_set_size(voltage_spinbox, 32, 10);

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
        current_spinbox = lv_spinbox_create(lv_screen_active());
        lv_spinbox_set_range(current_spinbox, 0, 400);
        lv_spinbox_set_digit_format(current_spinbox, 3, 1);
        lv_spinbox_set_step(current_spinbox, 1);

        lv_obj_set_content_height(current_spinbox, 12);

        lv_obj_set_style_pad_all(current_spinbox, -1, 0);
        lv_obj_set_style_border_width(current_spinbox, 0, 0);

        lv_obj_set_size(current_spinbox, 26, 10);

        lv_obj_set_style_outline_opa(current_spinbox, 0, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_opa(current_spinbox, 0, LV_STATE_EDITED);

        lv_obj_set_style_bg_color(current_spinbox, lv_color_black(), LV_PART_CURSOR | LV_STATE_EDITED);
        lv_obj_set_style_text_color(current_spinbox, lv_color_white(), LV_PART_CURSOR | LV_STATE_EDITED);
        lv_obj_set_style_bg_color(current_spinbox, lv_color_white(), LV_PART_CURSOR);
        lv_obj_set_style_text_color(current_spinbox, lv_color_black(), LV_PART_CURSOR);

        lv_obj_set_style_text_align(current_spinbox, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_align(current_spinbox, LV_ALIGN_TOP_RIGHT, -9, 18);
        lv_group_add_obj(group, current_spinbox);
        lv_obj_add_event_cb(current_spinbox, lvgl_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // 创建焦点高亮框
        highlight_frame = lv_obj_create(lv_screen_active());
        lv_obj_set_size(highlight_frame, 35, 14);
        lv_obj_align(highlight_frame, LV_ALIGN_TOP_LEFT, 34, 16);

        // 设置高亮框样式
        lv_obj_set_style_bg_opa(highlight_frame, LV_OPA_TRANSP, 0); // 背景透明，不遮挡内容
        lv_obj_set_style_border_width(highlight_frame, 1, 0);
        lv_obj_set_style_border_color(highlight_frame, lv_color_black(), 0); // 黑色边框，适合白色背景
        lv_obj_set_style_border_opa(highlight_frame, LV_OPA_80, 0);
        lv_obj_set_style_radius(highlight_frame, 4, 0); // 增加圆角
        lv_obj_set_style_pad_all(highlight_frame, 0, 0);

        // 设置高亮框不可点击，避免干扰交互，并移到后层避免遮挡
        lv_obj_add_flag(highlight_frame, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(highlight_frame, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_to_index(highlight_frame, 0); // 移到最底层

        // 为spinbox控件添加焦点事件
        lv_obj_add_event_cb(voltage_spinbox, focus_event_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(current_spinbox, focus_event_cb, LV_EVENT_FOCUSED, NULL);
        lvgl_port_unlock();
    }
}
