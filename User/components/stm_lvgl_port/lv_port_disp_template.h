/**
 * @file lv_port_disp_template.h
 *
 */

#ifndef LV_PORT_DISP_TEMPL_H
#define LV_PORT_DISP_TEMPL_H


/*********************
 *      INCLUDES
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
 lv_coord_t spinbox_x;
 lv_coord_t spinbox_width;
 lv_coord_t spinbox1_x;
 lv_coord_t spinbox1_width;
} focus_anim_config_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/* Initialize low level display driver */

void display_init(void);

void lv_port_disp_init(void);

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void);

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void);

bool lvgl_port_lock(uint32_t timeout_ms);

void lvgl_port_unlock(void);


/**********************
 *      MACROS
 **********************/

#endif /*LV_PORT_DISP_TEMPL_H*/
