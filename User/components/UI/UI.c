#include <stdio.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lv_port_encoder.h"
#include "tim.h"
#include "PID.h"
#include "hrtim.h"

static lv_indev_t *indev = NULL;
lv_group_t *group = NULL;

static lv_obj_t *voltage_label1 = NULL;
static lv_obj_t *current_label1 = NULL;
static lv_obj_t *power_value_label1 = NULL;
static lv_obj_t *voltage_label2 = NULL;
static lv_obj_t *current_label2 = NULL;
static lv_obj_t *power_value_label2 = NULL;
static lv_obj_t *status_label = NULL;

static lv_obj_t *mode_btns[4];
lv_obj_t *voltage_spinbox = NULL;
lv_obj_t *current_spinbox = NULL;

static lv_style_t style_spinbox_main;
static lv_style_t style_spinbox_focus;
static lv_style_t style_spinbox_edited;
static lv_style_t style_spinbox_cursor;
static lv_style_t style_spinbox_cursor_edited;

static char value_buf[2][3][32] = {{"10.00", "1.00", "10.00"}, {"15.00", "2.00", "30.00"}};
static const char *status_buf[][4] = {
    {"-", "预充电", "可调恒流", "恒压"},
    {"反向充电", "正向放电"}
};
static const char *mode_text[] = {"-", "→", "←", "↔", NULL};
// static char status_buf[16] = "-";
mode_t current_mode = MODE_1TO2;

static void init_spinbox_style(void)
{
    static bool inited = false;
    if(inited)
        return;

    lv_style_init(&style_spinbox_main);
    lv_style_set_pad_all(&style_spinbox_main, -1);
    lv_style_set_border_width(&style_spinbox_main, 1);
    lv_style_set_border_side(&style_spinbox_main, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_opa(&style_spinbox_main, 0);
    lv_style_set_radius(&style_spinbox_main, 0);
    lv_style_set_text_align(&style_spinbox_main, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_spinbox_focus);
    lv_style_set_outline_width(&style_spinbox_focus, 0);
    lv_style_set_border_opa(&style_spinbox_focus, LV_OPA_COVER);
    lv_style_set_border_width(&style_spinbox_focus, 1);
    lv_style_set_border_color(&style_spinbox_focus, lv_color_black());

    lv_style_init(&style_spinbox_edited);
    lv_style_set_outline_width(&style_spinbox_edited, 0);

    lv_style_init(&style_spinbox_cursor);
    lv_style_set_bg_color(&style_spinbox_cursor, lv_color_white());
    lv_style_set_text_color(&style_spinbox_cursor, lv_color_black());

    lv_style_init(&style_spinbox_cursor_edited);
    lv_style_set_bg_color(&style_spinbox_cursor_edited, lv_color_black());
    lv_style_set_text_color(&style_spinbox_cursor_edited, lv_color_white());
    lv_style_set_y(&style_spinbox_cursor_edited, 12);

    inited = true;
}

static void apply_spinbox_style(lv_obj_t *spinbox)
{
    lv_obj_add_style(spinbox, &style_spinbox_main, 0);
    lv_obj_add_style(spinbox, &style_spinbox_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(spinbox, &style_spinbox_edited, LV_STATE_EDITED);
    lv_obj_add_style(spinbox, &style_spinbox_cursor, LV_PART_CURSOR);
    lv_obj_add_style(spinbox, &style_spinbox_cursor_edited, LV_PART_CURSOR | LV_STATE_EDITED);
}

static void lvgl_event_cb(lv_event_t *evt)
{
    // static uint32_t freq = 100000;
    // static int16_t phase = 0;
    int32_t value = lv_spinbox_get_value(lv_event_get_current_target(evt));
    // // ESP_LOGI(TAG, "Value changed: %ld", value);
    // if(lv_event_get_current_target(evt) == voltage_spinbox)
    // {
    //     freq = value * 1000;
    //
    //     // pid_set_voltage(value * 10);
    pid_set_current(value * 10);
    // }
}

static void value_update_task(void *arg)
{
    while(1)
    {
        vTaskDelay(100);

        float high_voltage = get_pid_value(0) / 1000.0f;
        float high_current = get_pid_value(1) / 1000.0f;
        float low_voltage = get_pid_value(2) / 1000.0f;
        float low_current = get_pid_value(3) / 1000.0f;
        uint16_t status = get_pid_value(4);
        uint8_t mode = status & 0xF;
        uint8_t submode = status >> 8;
        uint8_t status_index = 0;

        snprintf(value_buf[0][0], 6, "%5.2f", high_voltage);
        snprintf(value_buf[0][1], 6, "%5.2f", high_current);
        snprintf(value_buf[0][2], 6, "%5.2f", high_voltage * high_current);

        snprintf(value_buf[1][0], 6, "%5.2f", low_voltage);
        snprintf(value_buf[1][1], 6, "%5.2f", low_current);
        snprintf(value_buf[1][2], 6, "%5.2f", low_voltage * low_current);

        status_index = 0;
        if(mode == MODE_AUTO)
            status_index = 1;

        if(lvgl_port_lock(portMAX_DELAY))
        {
            lv_label_set_text_static(voltage_label1, value_buf[0][0]);
            lv_label_set_text_static(current_label1, value_buf[0][1]);
            lv_label_set_text_static(power_value_label1, value_buf[0][2]);

            lv_label_set_text_static(voltage_label2, value_buf[1][0]);
            lv_label_set_text_static(current_label2, value_buf[1][1]);
            lv_label_set_text_static(power_value_label2, value_buf[1][2]);

            lv_label_set_text_static(status_label, status_buf[status_index][submode]);
            lvgl_port_unlock();
        }
    }
}

static void mode_btn_event_cb(lv_event_t *e)
{
    extern QueueHandle_t pid_ctrl_queue_mA;
    lv_obj_t *obj = lv_event_get_current_target(e);
    int32_t id = -1;
    for(int i = 0; i < 4; i++)
    {
        if(mode_btns[i] == obj)
        {
            lv_obj_add_state(mode_btns[i], LV_STATE_CHECKED);
            id = i;
        }
        else
        {
            lv_obj_remove_state(mode_btns[i], LV_STATE_CHECKED);
        }
    }
    if(id >= 0)
    {
        LOGI("BTN", "%ld", id);
    }
    id <<= 16;
    xQueueSend(pid_ctrl_queue_mA, &id, portMAX_DELAY);
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
        init_spinbox_style();

        lv_obj_t *label1 = lv_label_create(lv_screen_active());
        lv_label_set_text(label1, "端口1   端口2");
        lv_obj_set_width(label1, 78);
        lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(label1, LV_ALIGN_TOP_LEFT, 48, 0);

        lv_obj_t *label2 = lv_label_create(lv_screen_active());
        lv_label_set_text(label2, "电压/V");
        lv_obj_set_width(label2, 36);
        lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label2, LV_ALIGN_TOP_LEFT, 0, 16);

        lv_obj_t *label3 = lv_label_create(lv_screen_active());
        lv_label_set_text(label3, "电流/A");
        lv_obj_set_width(label3, 36);
        lv_obj_set_style_text_align(label3, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label3, LV_ALIGN_TOP_LEFT, 0, 32);

        lv_obj_t *label4 = lv_label_create(lv_screen_active());
        lv_label_set_text(label4, "功率/W");
        lv_obj_set_width(label4, 36);
        lv_obj_set_style_text_align(label4, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(label4, LV_ALIGN_TOP_LEFT, 0, 48);

        lv_obj_t *label5 = lv_label_create(lv_screen_active());
        lv_label_set_text(label5, "模式");
        lv_obj_set_width(label5, 24);
        lv_obj_set_style_text_align(label5, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(label5, LV_ALIGN_TOP_LEFT, 0, 80);

        lv_obj_t *label6 = lv_label_create(lv_screen_active());
        lv_label_set_text(label6, "状态");
        lv_obj_set_width(label6, 24);
        lv_obj_set_style_text_align(label6, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(label6, LV_ALIGN_TOP_LEFT, 0, 96);

        lv_obj_t *label7 = lv_label_create(lv_screen_active());
        lv_label_set_text(label7, "设定电流/A");
        lv_obj_set_width(label7, 60);
        lv_obj_set_style_text_align(label7, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(label7, LV_ALIGN_TOP_LEFT, 0, 112);

        // 电压数值
        voltage_label1 = lv_label_create(lv_screen_active());
        lv_label_set_text_static(voltage_label1, value_buf[0][0]);
        lv_obj_set_width(voltage_label1, 30);
        lv_obj_set_style_text_align(voltage_label1, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(voltage_label1, LV_ALIGN_TOP_LEFT, 48, 16);

        voltage_label2 = lv_label_create(lv_screen_active());
        lv_label_set_text_static(voltage_label2, value_buf[1][0]);
        lv_obj_set_width(voltage_label2, 30);
        lv_obj_set_style_text_align(voltage_label2, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(voltage_label2, LV_ALIGN_TOP_LEFT, 96, 16);

        //电流数值
        current_label1 = lv_label_create(lv_screen_active());
        lv_label_set_text_static(current_label1, value_buf[0][1]);
        lv_obj_set_width(current_label1, 30);
        lv_obj_set_style_text_align(current_label1, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(current_label1, LV_ALIGN_TOP_LEFT, 48, 32);

        current_label2 = lv_label_create(lv_screen_active());
        lv_label_set_text_static(current_label2, value_buf[1][1]);
        lv_obj_set_width(current_label2, 30);
        lv_obj_set_style_text_align(current_label2, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(current_label2, LV_ALIGN_TOP_LEFT, 96, 32);

        //功率数值
        power_value_label1 = lv_label_create(lv_screen_active());
        lv_label_set_text(power_value_label1, value_buf[0][2]);
        lv_obj_set_width(power_value_label1, 30);
        lv_obj_set_style_text_align(power_value_label1, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(power_value_label1, LV_ALIGN_TOP_LEFT, 48, 48);

        power_value_label2 = lv_label_create(lv_screen_active());
        lv_label_set_text(power_value_label2, value_buf[1][2]);
        lv_obj_set_width(power_value_label2, 30);
        lv_obj_set_style_text_align(power_value_label2, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(power_value_label2, LV_ALIGN_TOP_LEFT, 96, 48);

        status_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(status_label, status_buf[0][0]);
        lv_obj_set_width(status_label, 48);
        lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 64, 96);

        // --------- 四个独立模式按钮 ---------
        for(int i = 0; i < 4; i++)
        {
            mode_btns[i] = lv_button_create(lv_screen_active());
            lv_obj_set_size(mode_btns[i], 12, 12); // 独立按钮尺寸
            lv_obj_align(mode_btns[i], LV_ALIGN_TOP_LEFT, 48 + i * 18, 80);

            // 去除按钮的默认背景、轮廓和投影
            lv_obj_set_style_bg_opa(mode_btns[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_opa(mode_btns[i], LV_OPA_TRANSP, LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(mode_btns[i], LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
            lv_obj_set_style_bg_opa(mode_btns[i], LV_OPA_TRANSP, LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);

            lv_obj_set_style_shadow_width(mode_btns[i], 0, 0);
            lv_obj_set_style_outline_width(mode_btns[i], 0, LV_STATE_FOCUS_KEY);
            lv_obj_set_style_outline_width(mode_btns[i], 0, LV_STATE_EDITED);

            // 文本和圆角等
            lv_obj_set_style_radius(mode_btns[i], 0, 0);
            lv_obj_set_style_text_color(mode_btns[i], lv_color_black(), 0);
            lv_obj_set_style_text_color(mode_btns[i], lv_color_black(), LV_STATE_CHECKED);
            lv_obj_set_style_text_color(mode_btns[i], lv_color_black(), LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
            lv_obj_set_style_pad_all(mode_btns[i], 0, 0);
            // lv_obj_set_style_pad_bottom(mode_btns[i], 1, 0);

            // 默认透明边框占位 (防下划线出现/消失时抖动)
            lv_obj_set_style_border_width(mode_btns[i], 1, 0);
            lv_obj_set_style_border_side(mode_btns[i], LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_opa(mode_btns[i], 0, 0);

            // 聚焦时(指针指向): 下划线
            lv_obj_set_style_border_opa(mode_btns[i], LV_OPA_COVER, LV_STATE_FOCUS_KEY);
            lv_obj_set_style_border_width(mode_btns[i], 1, LV_STATE_FOCUS_KEY);
            lv_obj_set_style_border_color(mode_btns[i], lv_color_black(),
                                          LV_STATE_FOCUS_KEY);

            // 选中时(选中): 下划线
            lv_obj_set_style_border_opa(mode_btns[i], LV_OPA_COVER, LV_STATE_CHECKED);
            lv_obj_set_style_border_width(mode_btns[i], 2, LV_STATE_CHECKED);
            lv_obj_set_style_border_color(mode_btns[i], lv_color_black(), LV_STATE_CHECKED);
            //
            // 同时聚焦和选中时，使用更粗的下划线代替反色，防止白色下划线在透明背景下看不清
            lv_obj_set_style_border_opa(mode_btns[i], LV_OPA_COVER, LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
            lv_obj_set_style_border_width(mode_btns[i], 3, LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
            lv_obj_set_style_border_color(mode_btns[i], lv_color_black(), LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);

            lv_obj_t *lbl = lv_label_create(mode_btns[i]);
            lv_label_set_text(lbl, mode_text[i]);
            lv_obj_center(lbl);

            lv_obj_add_flag(mode_btns[i], LV_OBJ_FLAG_CHECKABLE);
            lv_group_add_obj(group, mode_btns[i]);
            lv_obj_add_event_cb(mode_btns[i], mode_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }
        // 默认选中第一个
        lv_obj_add_state(mode_btns[0], LV_STATE_CHECKED);

        //电流调整框
        current_spinbox = lv_spinbox_create(lv_screen_active());
        lv_spinbox_set_range(current_spinbox, 0, 310);
        lv_spinbox_set_digit_format(current_spinbox, 3, 1);
        lv_spinbox_set_step(current_spinbox, 1);
        lv_spinbox_set_value(current_spinbox, 0);

        lv_obj_set_content_height(current_spinbox, 12);
        lv_obj_set_size(current_spinbox, 32, 12);

        apply_spinbox_style(current_spinbox);

        lv_obj_align(current_spinbox, LV_ALIGN_TOP_LEFT, 72, 113);
        lv_group_add_obj(group, current_spinbox);
        lv_obj_add_event_cb(current_spinbox, lvgl_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // // 主动发送一个值改变事件以应用初值
        // lv_obj_send_event(voltage_spinbox, LV_EVENT_VALUE_CHANGED, NULL);
        // lv_obj_send_event(current_spinbox, LV_EVENT_VALUE_CHANGED, NULL);

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
