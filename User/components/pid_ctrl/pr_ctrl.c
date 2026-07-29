#include "pr_ctrl.h"

#include <math.h>
#include <stddef.h>

#define TWO_PI_F 6.2831853071795864769f

bool pr_ctrl_configure(pr_ctrl_t *ctrl,
                       float kp,
                       float kr,
                       float resonant_hz,
                       float bandwidth_hz,
                       float sample_hz,
                       float output_limit)
{
    if((ctrl == NULL) ||
       (kr < 0.0f) ||
       (resonant_hz <= 0.0f) ||
       (bandwidth_hz <= 0.0f) ||
       (sample_hz <= 2.0f * resonant_hz) ||
       (output_limit <= 0.0f))
    {
        return false;
    }

    const float w0 = TWO_PI_F * resonant_hz;
    const float wc = TWO_PI_F * bandwidth_hz;
    const float k = 2.0f * sample_hz;
    const float k2 = k * k;
    const float w02 = w0 * w0;
    const float denominator = k2 + 2.0f * wc * k + w02;
    const float numerator = 2.0f * kr * wc * k;

    ctrl->kp = kp;
    ctrl->b0 = numerator / denominator;
    ctrl->b2 = -ctrl->b0;
    ctrl->a1 = (-2.0f * k2 + 2.0f * w02) / denominator;
    ctrl->a2 = (k2 - 2.0f * wc * k + w02) / denominator;
    ctrl->output_limit = output_limit;
    pr_ctrl_reset(ctrl);
    return true;
}

void pr_ctrl_reset(pr_ctrl_t *ctrl)
{
    if(ctrl == NULL)
        return;

    ctrl->error_z1 = 0.0f;
    ctrl->error_z2 = 0.0f;
    ctrl->resonant_z1 = 0.0f;
    ctrl->resonant_z2 = 0.0f;
}

float pr_ctrl_compute(pr_ctrl_t *ctrl, float error)
{
    float resonant = ctrl->b0 * error +
                     ctrl->b2 * ctrl->error_z2 -
                     ctrl->a1 * ctrl->resonant_z1 -
                     ctrl->a2 * ctrl->resonant_z2;

    ctrl->error_z2 = ctrl->error_z1;
    ctrl->error_z1 = error;
    ctrl->resonant_z2 = ctrl->resonant_z1;
    ctrl->resonant_z1 = resonant;

    float output = ctrl->kp * error + resonant;
    if(output > ctrl->output_limit)
        output = ctrl->output_limit;
    else if(output < -ctrl->output_limit)
        output = -ctrl->output_limit;

    return output;
}
