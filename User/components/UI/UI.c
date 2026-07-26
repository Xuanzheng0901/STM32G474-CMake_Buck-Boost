#include <stdio.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lv_port_encoder.h"
#include "tim.h"
#include "PID.h"

LV_FONT_DECLARE(chillbit);

extern lv_obj_t *highlight_frame;
extern lv_anim_t focus_anim;

extern void focus_event_cb(lv_event_t *e);

static lv_indev_t *indev = NULL;
lv_group_t *group = NULL;

static lv_obj_t *va_label = NULL;
static lv_obj_t *vb_label = NULL;
static lv_obj_t *vc_label = NULL;
static lv_obj_t *ia_label = NULL;
static lv_obj_t *ib_label = NULL;
static lv_obj_t *ic_label = NULL;
static lv_obj_t *power_label = NULL;
lv_obj_t *voltage_spinbox = NULL;

static char buf_va[8] = "  0.0";
static char buf_vb[8] = "  0.0";
static char buf_vc[8] = "  0.0";
static char buf_ia[6] = "0.00";
static char buf_ib[6] = "0.00";
static char buf_ic[6] = "0.00";
static char buf_power[10] = "   0.0W";

static void lvgl_event_cb(lv_event_t *evt)
{
    int32_t value = lv_spinbox_get_value(lv_event_get_current_target(evt));
    if(lv_event_get_current_target(evt) == voltage_spinbox)
    {
        pid_set_voltage(value * 10);
    }
}

static void value_update_task(void *arg)
{
    extern float now_voltage_mV[3]; // [0]=Va, [1]=Vb, [2]=Vc (mV)
    extern float now_current_A[3]; // [0]=Ia, [1]=Ib, [2]=Ic (A)
    while(1)
    {
        vTaskDelay(100);

        // index: 0=Va, 1=Vb, 2=Vc (mV→V), 3=Ia, 4=Ib, 5=Ic (A)
        float va = now_voltage_mV[0] / 1000.0f;
        float vb = now_voltage_mV[1] / 1000.0f;
        float vc = now_voltage_mV[2] / 1000.0f;
        float ia = now_current_A[0];
        float ib = now_current_A[1];
        float ic = now_current_A[2];

        snprintf(buf_va, sizeof(buf_va), "%5.2f", va);
        snprintf(buf_vb, sizeof(buf_vb), "%5.2f", vb);
        snprintf(buf_vc, sizeof(buf_vc), "%5.2f", vc);
        snprintf(buf_ia, sizeof(buf_ia), "%4.2f", ia);
        snprintf(buf_ib, sizeof(buf_ib), "%4.2f", ib);
        snprintf(buf_ic, sizeof(buf_ic), "%4.2f", ic);
        snprintf(buf_power, sizeof(buf_power), "%6.2fW", va * ia + vb * ib + vc * ic);

        if(lvgl_port_lock(portMAX_DELAY))
        {
            lv_label_set_text_static(va_label, buf_va);
            lv_label_set_text_static(vb_label, buf_vb);
            lv_label_set_text_static(vc_label, buf_vc);
            lv_label_set_text_static(ia_label, buf_ia);
            lv_label_set_text_static(ib_label, buf_ib);
            lv_label_set_text_static(ic_label, buf_ic);
            lv_label_set_text_static(power_label, buf_power);
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
        // ====== y=0: 表头 ======
        lv_obj_t *label1 = lv_label_create(lv_screen_active());
        lv_label_set_text(label1, "电压(V)  电流(A)");
        lv_obj_set_width(label1, 128);
        lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(label1, LV_ALIGN_TOP_LEFT, 0, 0);

        // ====== y=16: A相 ======
        lv_obj_t *label2 = lv_label_create(lv_screen_active());
        lv_label_set_text(label2, "A相");
        lv_obj_set_width(label2, 24);
        lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label2, LV_ALIGN_TOP_LEFT, 0, 16);

        va_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(va_label, buf_va);
        lv_obj_set_width(va_label, 30);
        lv_obj_set_style_text_align(va_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(va_label, LV_ALIGN_TOP_LEFT, 37, 16);

        ia_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(ia_label, buf_ia);
        lv_obj_set_width(ia_label, 24);
        lv_obj_set_style_text_align(ia_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(ia_label, LV_ALIGN_TOP_LEFT, 94, 16);

        // ====== y=32: B相 ======
        lv_obj_t *label3 = lv_label_create(lv_screen_active());
        lv_label_set_text(label3, "B相");
        lv_obj_set_width(label3, 24);
        lv_obj_set_style_text_align(label3, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label3, LV_ALIGN_TOP_LEFT, 0, 32);

        vb_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(vb_label, buf_vb);
        lv_obj_set_width(vb_label, 30);
        lv_obj_set_style_text_align(vb_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(vb_label, LV_ALIGN_TOP_LEFT, 37, 32);

        ib_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(ib_label, buf_ib);
        lv_obj_set_width(ib_label, 24);
        lv_obj_set_style_text_align(ib_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(ib_label, LV_ALIGN_TOP_LEFT, 94, 32);

        // ====== y=48: C相  ======
        lv_obj_t *power_label_text = lv_label_create(lv_screen_active());
        lv_label_set_text(power_label_text, "C相");
        lv_obj_set_width(power_label_text, 24);
        lv_obj_set_style_text_align(power_label_text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(power_label_text, LV_ALIGN_TOP_LEFT, 0, 48);

        vc_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(vc_label, buf_vc);
        lv_obj_set_width(vc_label, 30);
        lv_obj_set_style_text_align(vc_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(vc_label, LV_ALIGN_TOP_LEFT, 37, 48);

        ic_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(ic_label, buf_ic);
        lv_obj_set_width(ic_label, 24);
        lv_obj_set_style_text_align(ic_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(ic_label, LV_ALIGN_TOP_LEFT, 94, 48);

        // 设定
        lv_obj_t *set_label = lv_label_create(lv_screen_active());
        lv_label_set_text(set_label, "设定");
        lv_obj_set_width(set_label, 24);
        lv_obj_set_style_text_align(set_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(set_label, LV_ALIGN_TOP_LEFT, 0, 64);

        voltage_spinbox = lv_spinbox_create(lv_screen_active());
        lv_spinbox_set_range(voltage_spinbox, 0, 2500);
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

        lv_obj_align(voltage_spinbox, LV_ALIGN_TOP_LEFT, 36, 65);
        lv_group_add_obj(group, voltage_spinbox);
        lv_obj_add_event_cb(voltage_spinbox, lvgl_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // 功率
        lv_obj_t *p_label = lv_label_create(lv_screen_active());
        lv_label_set_text(p_label, "功率");
        lv_obj_set_width(p_label, 24);
        lv_obj_set_style_text_align(p_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(p_label, LV_ALIGN_TOP_LEFT, 0, 80);

        power_label = lv_label_create(lv_screen_active());
        lv_label_set_text_static(power_label, buf_power);
        lv_obj_set_width(power_label, 54);
        lv_obj_set_style_text_align(power_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(power_label, LV_ALIGN_TOP_LEFT, 24, 80);

        // 页脚
        lv_obj_t *my_label = lv_label_create(lv_scr_act());
        lv_label_set_text(my_label, "三相逆变器");
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
    LOGI("LVGL", "Hello LVGL!");
    if(xTaskCreate(value_update_task, "update value", 512, NULL, 10, NULL) != pdPASS)
    {
        printf("update value task creation failed\n");
    }
}
