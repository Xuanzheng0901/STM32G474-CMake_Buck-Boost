/**
 * @file    ctrl_loop_state.c
 * @brief   PFC 状态机: 软启动斜坡 + 故障保护
 */

#include "ctrl_loop_state.h"
#include "pfc_config.h"

/* ==================== 内部状态 ==================== */

static CtrlLoop_State state;
static float32_t vref_ramp;
static float32_t vout_target;
static uint32_t soft_start_ticks;
static uint32_t soft_start_total;

/* ==================== API 实现 ==================== */

void ctrl_loop_state_init(float32_t target, uint32_t ramp_ticks)
{
    state = CTRL_STATE_SOFT_START;
    vout_target = target;
    vref_ramp = 0.0f;
    soft_start_ticks = 0;
    soft_start_total = ramp_ticks;
}

float32_t ctrl_loop_state_update(float32_t vout, float32_t iout)
{
    /* 故障检测 */
    if (state != CTRL_STATE_FAULT) {
        if (vout > vout_target * PFC_OVP_RATIO || iout > PFC_OCP_AMPS) {
            state = CTRL_STATE_FAULT;
            vref_ramp = 0.0f;
            return 0.0f;
        }
    }

    if (state == CTRL_STATE_IDLE || state == CTRL_STATE_FAULT)
        return 0.0f;

    if (state == CTRL_STATE_SOFT_START) {
        soft_start_ticks++;
        vref_ramp = vout_target * (float32_t)soft_start_ticks / (float32_t)soft_start_total;
        if (soft_start_ticks >= soft_start_total) {
            vref_ramp = vout_target;
            state = CTRL_STATE_RUNNING;
        }
    }
    return vref_ramp;
}

CtrlLoop_State ctrl_loop_state_get(void)    { return state; }

float32_t ctrl_loop_state_get_vref(void)     { return vref_ramp; }
float32_t ctrl_loop_state_get_vout_target(void) { return vout_target; }

void ctrl_loop_state_set_vout(float32_t vout)
{
    if (vout < 1.0f) vout = 1.0f;
    float32_t ratio = (vout_target > 0) ? vref_ramp / vout_target : 0.0f;
    vout_target = vout;
    if (state == CTRL_STATE_RUNNING) {
        state = CTRL_STATE_SOFT_START;
        soft_start_ticks = (uint32_t)(ratio * (float32_t)soft_start_total);
    }
}

void ctrl_loop_state_enter_fault(void)
{
    state = CTRL_STATE_FAULT;
    vref_ramp = 0.0f;
}

void ctrl_loop_state_clear_fault(void)
{
    state = CTRL_STATE_SOFT_START;
    soft_start_ticks = 0;
    vref_ramp = 0.0f;
}
