/**
 * @file    ctrl_loop_state.h
 * @brief   PFC 控制回路状态机: 软启动/运行/故障保护
 */

#ifndef __CTRL_LOOP_STATE_H__
#define __CTRL_LOOP_STATE_H__

#include "arm_math.h"

typedef enum {
    CTRL_STATE_IDLE = 0,
    CTRL_STATE_SOFT_START,
    CTRL_STATE_RUNNING,
    CTRL_STATE_FAULT
} CtrlLoop_State;

/* ---- API ---- */

void     ctrl_loop_state_init(float32_t vout_target, uint32_t ramp_ticks);

/** 更新状态机并检测故障, 返回当前 Vref 供 PI 计算使用 */
float32_t ctrl_loop_state_update(float32_t vout, float32_t iout);

CtrlLoop_State ctrl_loop_state_get(void);
void     ctrl_loop_state_set_vout(float32_t vout);
void     ctrl_loop_state_enter_fault(void);
void     ctrl_loop_state_clear_fault(void);
float32_t ctrl_loop_state_get_vref(void);
float32_t ctrl_loop_state_get_vout_target(void);

#endif
