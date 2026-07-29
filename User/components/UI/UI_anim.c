#include "lvgl.h"

extern lv_obj_t *line_voltage_spinbox;
extern lv_obj_t *frequency_30_button;
extern lv_obj_t *frequency_60_button;

lv_obj_t *highlight_frame = NULL;

void focus_event_cb(lv_event_t *event)
{
    lv_obj_t *object = lv_event_get_target(event);

    if(highlight_frame == NULL ||
       (object != line_voltage_spinbox && object != frequency_30_button && object != frequency_60_button))
        return;

    lv_obj_set_x(highlight_frame, lv_obj_get_x(object));
    lv_obj_set_y(highlight_frame, object == line_voltage_spinbox ? 77 : 93);
    lv_obj_set_width(highlight_frame, lv_obj_get_width(object));
}
