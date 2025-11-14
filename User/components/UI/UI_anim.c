#include "lvgl.h"
#include "../stm_lvgl_port/lv_port_disp_template.h"

extern lv_group_t *group;
extern lv_obj_t *voltage_spinbox;
extern lv_obj_t *current_spinbox;


lv_obj_t *highlight_frame = NULL;
lv_anim_t focus_anim;
bool is_animating = false;

static const focus_anim_config_t anim_config = {
    .spinbox_x = 34,
    .spinbox_width = 35,
    .spinbox1_x = 91,
    .spinbox1_width = 29
};

// 统一的动画回调函数
static void focus_anim_exec_cb(void *var, int32_t v)
{
    lv_obj_t *frame = (lv_obj_t *)var;
    lv_obj_set_width(frame, v);
}

// 通用的位置宽度同步动画回调
static void focus_sync_anim_exec_cb(void *var, int32_t v)
{
    lv_obj_t *frame = (lv_obj_t *)var;
    lv_obj_t *focused_obj = lv_group_get_focused(group);

    static lv_coord_t initial_width = 0;
    static lv_coord_t initial_x = 0;
    static lv_coord_t target_x = 0;
    static lv_coord_t target_width = 0;

    // 初始化静态变量
    if(initial_width == 0)
    {
        initial_width = lv_obj_get_width(frame);
        initial_x = lv_obj_get_x(frame);

        if(focused_obj == voltage_spinbox)
        {
            target_x = anim_config.spinbox_x;
            target_width = anim_config.spinbox_width;
        }
        else if(focused_obj == current_spinbox)
        {
            target_x = anim_config.spinbox1_x;
            target_width = anim_config.spinbox1_width;
        }
    }

    // 计算当前位置
    lv_coord_t current_x;
    if(focused_obj == voltage_spinbox && initial_x > target_x)
    {
        // 从右向左收缩
        current_x = target_x + (initial_width - v);
    }
    else if(focused_obj == current_spinbox && initial_x < target_x)
    {
        // 从左向右收缩
        float progress = (float)(initial_width - v) / (initial_width - target_width);
        current_x = initial_x + (target_x - initial_x) * progress;
    }
    else
    {
        current_x = target_x;
    }

    lv_obj_set_x(frame, current_x);
    lv_obj_set_width(frame, v);

    // 重置静态变量
    if(v == target_width)
    {
        initial_width = 0;
        initial_x = 0;
        target_x = 0;
        target_width = 0;
    }
}

// 向左扩展动画回调
static void focus_expand_left_exec_cb(void *var, int32_t v)
{
    lv_obj_t *frame = (lv_obj_t *)var;
    lv_coord_t current_x = lv_obj_get_x(frame);
    lv_coord_t current_width = lv_obj_get_width(frame);
    lv_coord_t width_increase = v - current_width;
    lv_obj_set_x(frame, current_x - width_increase);
    lv_obj_set_width(frame, v);
}

// 启动动画的封装函数
static void start_focus_animation(lv_anim_exec_xcb_t exec_cb, lv_coord_t from_val, lv_coord_t to_val,
                                  lv_anim_path_cb_t path_cb, lv_anim_ready_cb_t ready_cb, uint32_t duration)
{
    lv_anim_init(&focus_anim);
    lv_anim_set_var(&focus_anim, highlight_frame);
    lv_anim_set_exec_cb(&focus_anim, exec_cb);
    lv_anim_set_values(&focus_anim, from_val, to_val);
    lv_anim_set_duration(&focus_anim, duration);
    lv_anim_set_path_cb(&focus_anim, path_cb);
    if(ready_cb)
        lv_anim_set_ready_cb(&focus_anim, ready_cb);
    lv_anim_start(&focus_anim);
}

// 获取目标控件配置
static void get_target_config(lv_obj_t *obj, lv_coord_t *target_x, lv_coord_t *target_width)
{
    if(obj == voltage_spinbox)
    {
        *target_x = anim_config.spinbox_x;
        *target_width = anim_config.spinbox_width;
    }
    else if(obj == current_spinbox)
    {
        *target_x = anim_config.spinbox1_x;
        *target_width = anim_config.spinbox1_width;
    }
}

static void focus_anim_ready_cb(lv_anim_t *a)
{
    is_animating = false;
    // 第二阶段动画 - 收缩到目标位置
    lv_obj_t *focused_obj = lv_group_get_focused(group);
    if(focused_obj)
    {
        lv_coord_t target_x, target_width;
        get_target_config(focused_obj, &target_x, &target_width);

        // 获取当前位置和宽度
        lv_coord_t current_x = lv_obj_get_x(highlight_frame);
        lv_coord_t current_width = lv_obj_get_width(highlight_frame);

        // 判断是否需要同步位置宽度动画
        bool need_sync_anim = (focused_obj == voltage_spinbox && current_x < target_x) ||
                              (focused_obj == current_spinbox && current_x < target_x);

        if(need_sync_anim)
        {
            // 使用同步动画
            start_focus_animation(focus_sync_anim_exec_cb, current_width, target_width,
                                  lv_anim_path_ease_out, NULL, 200);
        }
        else
        {
            // 使用分离动画（后备方案）
            if(current_x != target_x)
            {
                lv_anim_t pos_anim;
                lv_anim_init(&pos_anim);
                lv_anim_set_var(&pos_anim, highlight_frame);
                lv_anim_set_exec_cb(&pos_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
                lv_anim_set_values(&pos_anim, current_x, target_x);
                lv_anim_set_duration(&pos_anim, 200);
                lv_anim_set_path_cb(&pos_anim, lv_anim_path_ease_out);
                lv_anim_start(&pos_anim);
            }

            if(current_width != target_width)
            {
                start_focus_animation(focus_anim_exec_cb, current_width, target_width,
                                      lv_anim_path_ease_out, NULL, 200);
            }
        }
    }
}

// 焦点事件处理函数
void focus_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if(code == LV_EVENT_FOCUSED && !is_animating)
    {
        is_animating = true;

        lv_coord_t current_x = lv_obj_get_x(highlight_frame);
        lv_coord_t current_width = lv_obj_get_width(highlight_frame);
        lv_coord_t target_x, target_width;

        get_target_config(obj, &target_x, &target_width);

        // 计算扩展参数
        lv_coord_t expand_width = current_width;
        lv_anim_exec_xcb_t expand_callback = focus_anim_exec_cb; // 默认向右扩展

        if(obj == voltage_spinbox && current_x > target_x)
        {
            // 从右向左切换：向左扩展
            expand_width = current_x - target_x + current_width;
            expand_callback = focus_expand_left_exec_cb;
        }
        else if(obj == current_spinbox && current_x < target_x)
        {
            // 从左向右切换：向右扩展
            expand_width = target_x - current_x + target_width;
            expand_callback = focus_anim_exec_cb;
        }

        // 执行扩展动画
        if(expand_width > current_width)
        {
            start_focus_animation(expand_callback, current_width, expand_width,
                                  lv_anim_path_ease_in, focus_anim_ready_cb, 200);
        }
        else
        {
            // 如果不需要扩展，直接执行收缩动画
            focus_anim_ready_cb(NULL);
        }
    }
}
