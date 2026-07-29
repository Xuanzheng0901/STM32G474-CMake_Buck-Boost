#pragma once

#include <stdbool.h>

/**
 * @brief 离散准 PR 控制器
 *
 * 连续域模型:
 * G(s) = Kp + 2*Kr*wc*s / (s^2 + 2*wc*s + w0^2)
 */
typedef struct {
    float kp;
    float b0;
    float b2;
    float a1;
    float a2;
    float error_z1;
    float error_z2;
    float resonant_z1;
    float resonant_z2;
    float output_limit;
} pr_ctrl_t;

bool pr_ctrl_configure(pr_ctrl_t *ctrl,
                       float kp,
                       float kr,
                       float resonant_hz,
                       float bandwidth_hz,
                       float sample_hz,
                       float output_limit);

void pr_ctrl_reset(pr_ctrl_t *ctrl);

float pr_ctrl_compute(pr_ctrl_t *ctrl, float error);
