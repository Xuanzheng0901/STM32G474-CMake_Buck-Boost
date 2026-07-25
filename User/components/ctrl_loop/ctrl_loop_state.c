/**
 * @file    ctrl_loop_state.c
 * @brief   PFC 状态机: 软启动斜坡 + 故障保护
 */

#include "ctrl_loop_state.h"

#include "pfc_config.h"
#include "sogi.h"
#include <stdbool.h>

/* ==================== 内部状态 ==================== */

static CtrlLoop_State state;
static float32_t vref_ramp;
static float32_t vout_target;
static uint32_t soft_start_ticks;
static uint32_t soft_start_total;

/* ==================== 故障检测 ==================== */

static bool check_fault(float32_t vout, float32_t iout)
{
    /* 过压 */
    if (vout > vout_target * PFC_OVP_RATIO)
        return true;

    /* 过流 */
    if (iout > PFC_OCP_AMPS)
        return true;

    /* 电网丢失: SOGI u_D(per-unit) 转实际 Vpeak → 与阈值比较 */
    if (spll.u_D[0] * PFC_SOGI_VOLT_PER_UNIT < PFC_UVP_VIN_RMS * 1.414f)
        return true;

    /* 软启动超时 */
    if (state == CTRL_STATE_SOFT_START) {
        uint32_t timeout_ticks = (uint32_t)(PFC_SS_TIMEOUT_SEC * PFC_VOLTAGE_LOOP_FREQ);
        if (soft_start_ticks > timeout_ticks)
            return true;
    }

    return false;
}

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
    if (state != CTRL_STATE_FAULT && check_fault(vout, iout)) {
        state = CTRL_STATE_FAULT;
        vref_ramp = 0.0f;
        return 0.0f;
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

CtrlLoop_State ctrl_loop_state_get(void)          { return state; }
float32_t ctrl_loop_state_get_vref(void)           { return vref_ramp; }
float32_t ctrl_loop_state_get_vout_target(void)    { return vout_target; }

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
