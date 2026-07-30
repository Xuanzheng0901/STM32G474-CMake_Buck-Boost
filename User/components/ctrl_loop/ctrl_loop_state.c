/**
 * @file    ctrl_loop_state.c
 * @brief   PFC startup and protection state machine
 */

#include "ctrl_loop_state.h"

#include "pfc_config.h"

#include <math.h>

static volatile CtrlLoop_State state;
static volatile PfcFaultReason fault_reason;
static float32_t vref_ramp;
static float32_t vref_start;
static float32_t vout_target;
static uint32_t soft_start_ticks;
static uint32_t soft_start_total;
static uint32_t vout_ready_ticks;
static uint32_t retry_wait_ticks;
static uint32_t retry_count;
static uint16_t grid_valid_cycles;
static uint16_t grid_lost_cycles;
static uint16_t pll_lock_cycles;
static uint16_t pll_lost_cycles;

static void reset_qualification(void)
{
    grid_valid_cycles = 0U;
    grid_lost_cycles = 0U;
    pll_lock_cycles = 0U;
    pll_lost_cycles = 0U;
}

static bool pll_is_locked(float32_t ud, float32_t uq, float32_t freq)
{
    float32_t abs_ud = fabsf(ud);

    return (ud >= PFC_PLL_LOCK_MIN_D_PU) &&
           (fabsf(freq - PFC_GRID_FREQ_HZ) <= PFC_PLL_LOCK_FREQ_TOL_HZ) &&
           (fabsf(uq) <= abs_ud * PFC_PLL_LOCK_Q_RATIO);
}

static void enter_wait_grid(void)
{
    state = CTRL_STATE_WAIT_GRID;
    vref_ramp = 0.0f;
    reset_qualification();
}

static void enter_soft_start(float32_t vout)
{
    if(vout < 0.0f)
        vout = 0.0f;
    if(vout > vout_target)
        vout = vout_target;

    state = CTRL_STATE_SOFT_START;
    vref_start = vout;
    vref_ramp = vout;
    soft_start_ticks = 0U;
    vout_ready_ticks = 0U;
    grid_lost_cycles = 0U;
    pll_lost_cycles = 0U;
}

static void enter_retry_wait(PfcFaultReason reason)
{
    state = CTRL_STATE_RETRY_WAIT;
    fault_reason = reason;
    vref_ramp = 0.0f;
    retry_wait_ticks = 0U;
    retry_count++;
    reset_qualification();
}

static void enter_latched_fault(PfcFaultReason reason)
{
    state = CTRL_STATE_FAULT;
    fault_reason = reason;
    vref_ramp = 0.0f;
    reset_qualification();
}

static bool update_grid_loss(float32_t vin_rms, bool vin_cycle_valid)
{
    if(!vin_cycle_valid)
        return false;

    if(vin_rms < PFC_GRID_STOP_VRMS)
    {
        if(grid_lost_cycles < PFC_GRID_LOST_CYCLES)
            grid_lost_cycles++;
    }
    else
    {
        grid_lost_cycles = 0U;
    }

    return grid_lost_cycles >= PFC_GRID_LOST_CYCLES;
}

static bool update_pll_loss(float32_t ud,
                            float32_t uq,
                            float32_t freq,
                            bool vin_cycle_valid)
{
    if(!vin_cycle_valid)
        return false;

    if(pll_is_locked(ud, uq, freq))
    {
        pll_lost_cycles = 0U;
    }
    else if(pll_lost_cycles < PFC_PLL_LOST_CYCLES)
    {
        pll_lost_cycles++;
    }

    return pll_lost_cycles >= PFC_PLL_LOST_CYCLES;
}

void ctrl_loop_state_init(float32_t target, uint32_t ramp_ticks)
{
    state = CTRL_STATE_WAIT_GRID;
    fault_reason = PFC_FAULT_NONE;
    vref_ramp = 0.0f;
    vref_start = 0.0f;
    vout_target = target;
    soft_start_ticks = 0U;
    soft_start_total = (ramp_ticks > 0U) ? ramp_ticks : 1U;
    vout_ready_ticks = 0U;
    retry_wait_ticks = 0U;
    retry_count = 0U;
    reset_qualification();
}

float32_t ctrl_loop_state_update(float32_t vout,
                                 float32_t iout,
                                 float32_t vin_rms,
                                 bool vin_cycle_valid,
                                 float32_t pll_ud,
                                 float32_t pll_uq,
                                 float32_t pll_freq)
{
    if(state == CTRL_STATE_FAULT)
        return 0.0f;

    /* Bus overvoltage remains active in every initialized state. */
    if(vout > vout_target * PFC_OVP_RATIO)
    {
        enter_latched_fault(PFC_FAULT_VOUT_OV);
        return 0.0f;
    }

    /* The power stage is off before soft start, so OCP is only meaningful here. */
    if(((state == CTRL_STATE_SOFT_START) ||
        (state == CTRL_STATE_RUNNING)) &&
       (iout > PFC_OCP_AMPS))
    {
        enter_latched_fault(PFC_FAULT_IOUT_OC);
        return 0.0f;
    }

    switch(state)
    {
        case CTRL_STATE_WAIT_GRID:
            if(vin_cycle_valid)
            {
                if(vin_rms >= PFC_GRID_START_VRMS)
                {
                    if(grid_valid_cycles < PFC_GRID_START_CYCLES)
                        grid_valid_cycles++;
                }
                else
                {
                    grid_valid_cycles = 0U;
                }

                if(grid_valid_cycles >= PFC_GRID_START_CYCLES)
                {
                    state = CTRL_STATE_WAIT_PLL;
                    pll_lock_cycles = 0U;
                }
            }
            break;

        case CTRL_STATE_WAIT_PLL:
            if(vin_cycle_valid)
            {
                if(vin_rms < PFC_GRID_STOP_VRMS)
                {
                    enter_wait_grid();
                    break;
                }

                if(pll_is_locked(pll_ud, pll_uq, pll_freq))
                {
                    if(pll_lock_cycles < PFC_PLL_LOCK_CYCLES)
                        pll_lock_cycles++;
                }
                else
                {
                    pll_lock_cycles = 0U;
                }

                if(pll_lock_cycles >= PFC_PLL_LOCK_CYCLES)
                {
                    fault_reason = PFC_FAULT_NONE;
                    enter_soft_start(vout);
                }
            }
            break;

        case CTRL_STATE_SOFT_START:
            if(update_grid_loss(vin_rms, vin_cycle_valid))
            {
                enter_retry_wait(PFC_FAULT_GRID_UV);
                break;
            }
            if(update_pll_loss(pll_ud, pll_uq, pll_freq, vin_cycle_valid))
            {
                enter_retry_wait(PFC_FAULT_PLL_UNLOCK);
                break;
            }

            soft_start_ticks++;
            if(soft_start_ticks < soft_start_total)
            {
                float32_t progress =
                    (float32_t)soft_start_ticks / (float32_t)soft_start_total;
                vref_ramp =
                    vref_start + (vout_target - vref_start) * progress;
            }
            else
            {
                vref_ramp = vout_target;
            }

            if((soft_start_ticks >= soft_start_total) &&
               (fabsf(vout - vout_target) <= PFC_VOUT_READY_TOLERANCE_V))
            {
                if(vout_ready_ticks < PFC_VOUT_READY_TICKS)
                    vout_ready_ticks++;
                if(vout_ready_ticks >= PFC_VOUT_READY_TICKS)
                {
                    state = CTRL_STATE_RUNNING;
                    fault_reason = PFC_FAULT_NONE;
                    retry_count = 0U;
                }
            }
            else
            {
                vout_ready_ticks = 0U;
            }

            if((state == CTRL_STATE_SOFT_START) &&
               (soft_start_ticks >=
                (uint32_t)(PFC_SS_TIMEOUT_SEC * PFC_VOLTAGE_LOOP_FREQ)))
            {
                enter_retry_wait(PFC_FAULT_SOFTSTART_TIMEOUT);
            }
            break;

        case CTRL_STATE_RUNNING:
            vref_ramp = vout_target;
            if(update_grid_loss(vin_rms, vin_cycle_valid))
            {
                enter_retry_wait(PFC_FAULT_GRID_UV);
            }
            else if(update_pll_loss(pll_ud, pll_uq, pll_freq,
                                    vin_cycle_valid))
            {
                enter_retry_wait(PFC_FAULT_PLL_UNLOCK);
            }
            break;

        case CTRL_STATE_RETRY_WAIT:
            retry_wait_ticks++;
            if(retry_wait_ticks >=
               (uint32_t)(PFC_FAULT_RETRY_MS *
                          PFC_VOLTAGE_LOOP_FREQ / 1000.0f))
            {
                enter_wait_grid();
            }
            break;

        case CTRL_STATE_FAULT:
        default:
            break;
    }

    return vref_ramp;
}

CtrlLoop_State ctrl_loop_state_get(void)
{
    return state;
}

float32_t ctrl_loop_state_get_vref(void)
{
    return vref_ramp;
}

float32_t ctrl_loop_state_get_vout_target(void)
{
    return vout_target;
}

PfcFaultReason ctrl_loop_state_get_fault_reason(void)
{
    return fault_reason;
}

uint32_t ctrl_loop_state_get_retry_count(void)
{
    return retry_count;
}

void ctrl_loop_state_set_vout(float32_t vout)
{
    if(vout < 1.0f)
        vout = 1.0f;

    vout_target = vout;
    if(state == CTRL_STATE_RUNNING)
        enter_soft_start(vref_ramp);
}

void ctrl_loop_state_clear_fault(void)
{
    fault_reason = PFC_FAULT_NONE;
    retry_count = 0U;
    enter_wait_grid();
}
