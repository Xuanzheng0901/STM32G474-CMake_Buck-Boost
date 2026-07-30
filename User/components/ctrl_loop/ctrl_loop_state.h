/**
 * @file    ctrl_loop_state.h
 * @brief   PFC 控制回路状态机: 软启动/运行/故障保护
 */

#ifndef __CTRL_LOOP_STATE_H__
#define __CTRL_LOOP_STATE_H__

#include <stdbool.h>
#include "arm_math.h"

typedef enum {
    CTRL_STATE_WAIT_GRID = 0,
    CTRL_STATE_WAIT_PLL,
    CTRL_STATE_SOFT_START,
    CTRL_STATE_RUNNING,
    CTRL_STATE_RETRY_WAIT,
    CTRL_STATE_FAULT
} CtrlLoop_State;

typedef enum {
    PFC_FAULT_NONE = 0,
    PFC_FAULT_GRID_UV,
    PFC_FAULT_PLL_UNLOCK,
    PFC_FAULT_SOFTSTART_TIMEOUT,
    PFC_FAULT_VOUT_OV,
    PFC_FAULT_IOUT_OC
} PfcFaultReason;

/* ---- API ---- */

void     ctrl_loop_state_init(float32_t vout_target, uint32_t ramp_ticks);

/** 更新状态机并检测故障, 返回当前 Vref 供 PI 计算使用 */
float32_t ctrl_loop_state_update(float32_t vout,
                                 float32_t iout,
                                 float32_t vin_rms,
                                 bool vin_cycle_valid,
                                 float32_t pll_ud,
                                 float32_t pll_uq,
                                 float32_t pll_freq);

CtrlLoop_State ctrl_loop_state_get(void);
void     ctrl_loop_state_set_vout(float32_t vout);
void     ctrl_loop_state_clear_fault(void);
float32_t ctrl_loop_state_get_vref(void);
float32_t ctrl_loop_state_get_vout_target(void);
PfcFaultReason ctrl_loop_state_get_fault_reason(void);
uint32_t ctrl_loop_state_get_retry_count(void);

#endif
