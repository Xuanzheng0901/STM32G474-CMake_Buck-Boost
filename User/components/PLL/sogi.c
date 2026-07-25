#include "sogi.h"

#include "dac.h"
#include "stm32g4xx_hal_dac.h"

SPLL_1PH_SOGI spll;

//复位SPLL参数
void SPLL_1PH_SOGI_reset(SPLL_1PH_SOGI *spll_obj)
{
    spll_obj->u[0] = (float32_t)(0.0);
    spll_obj->u[1] = (float32_t)(0.0);
    spll_obj->u[2] = (float32_t)(0.0);

    spll_obj->osg_u[0] = (float32_t)(0.0);
    spll_obj->osg_u[1] = (float32_t)(0.0);
    spll_obj->osg_u[2] = (float32_t)(0.0);

    spll_obj->osg_qu[0] = (float32_t)(0.0);
    spll_obj->osg_qu[1] = (float32_t)(0.0);
    spll_obj->osg_qu[2] = (float32_t)(0.0);

    spll_obj->u_Q[0] = (float32_t)(0.0);
    spll_obj->u_Q[1] = (float32_t)(0.0);

    spll_obj->u_D[0] = (float32_t)(0.0);
    spll_obj->u_D[1] = (float32_t)(0.0);

    spll_obj->ylf[0] = (float32_t)(0.0);
    spll_obj->ylf[1] = (float32_t)(0.0);

    spll_obj->fo = (float32_t)(0.0);

    spll_obj->theta = (float32_t)(0.0);

    spll_obj->sine = (float32_t)(0.0);
    spll_obj->cosine = (float32_t)(0.0);
}

//SOGI参数计算
void SPLL_1PH_SOGI_coeff_calc(SPLL_1PH_SOGI *spll_obj)
{
    float32_t osgx, osgy, temp, wn;
    // w = 2 * pi *f
    wn = spll_obj->fn * (float32_t)2.0f * (float32_t)3.14159265f;
    //设置SOGI中的K值，默认为0.5
    spll_obj->osg_coeff.osg_k = (float32_t)(0.5);
    //x = 2 * k * w_n * T_s
    osgx = (float32_t)(2.0f * spll_obj->osg_coeff.osg_k * wn * spll_obj->delta_t);
    spll_obj->osg_coeff.osg_x = osgx;
    //y = (w_n * T_s)^2
    osgy = (float32_t)(wn * spll_obj->delta_t * wn * spll_obj->delta_t);
    spll_obj->osg_coeff.osg_y = osgy;
    //temp = 1/ (x + y +4)
    temp = (float32_t)1.0 / (osgy + osgx + 4.0f);

    // b0 = x * temp
    spll_obj->osg_coeff.osg_b0 = ((float32_t)temp * osgx);
    // b2 = -b0
    spll_obj->osg_coeff.osg_b2 = ((float32_t)(-1.0f) * spll_obj->osg_coeff.osg_b0);
    //a1 = 2 * (4 - y) * temp      a2 = (x - y - 4) * temp
    spll_obj->osg_coeff.osg_a1 = ((float32_t)(2.0 * (4.0f - osgy)) * temp);
    spll_obj->osg_coeff.osg_a2 = ((float32_t)(osgx - osgy - 4.0f) * temp);
    //qb0 = k * y / (x+y+4)
    spll_obj->osg_coeff.osg_qb0 = ((float32_t)(spll_obj->osg_coeff.osg_k * osgy) * temp);
    //qb1 = 2 * k y / (x+y+4) = 2 * qb0
    spll_obj->osg_coeff.osg_qb1 = ((float32_t)(2.0) * spll_obj->osg_coeff.osg_qb0);
    //qb2 = k * y / (x+y+4) = qb0
    spll_obj->osg_coeff.osg_qb2 = spll_obj->osg_coeff.osg_qb0;
}

//配置PLL相关变量，主要包括网侧频率，PLL 中PI 参数
void SPLL_1PH_SOGI_config(SPLL_1PH_SOGI *spll_obj,
                          float32_t acFreq,
                          float32_t isrFrequency,
                          float32_t lpf_b0,
                          float32_t lpf_b1)
{
    spll_obj->fn = acFreq;
    spll_obj->delta_t = ((1.0f) / isrFrequency);

    SPLL_1PH_SOGI_coeff_calc(spll_obj);

    spll_obj->lpf_coeff.b0 = lpf_b0;
    spll_obj->lpf_coeff.b1 = lpf_b1;
}

//计算后返回相位
float32_t SPLL_1PH_SOGI_run(SPLL_1PH_SOGI *spll_obj, float32_t acValue)
{
    //更新输入缓冲（acValue 应为外部归一化后的电压值）
    spll_obj->u[0] = acValue;

    /*
     * 产生alpha beta正交信号
     * 离散计算公式： alpha Hd = (b0 * z^2 + b2) / (z^2 -a1 * z - a2)
     *              beta Hq = (qb0 * z^2 + qb1 * z + qb2) / (z^2 - a1 * z - a2)
     */

    //alpha信号    Uo = b0 * u[0] + b2 * u[2] + a1 * Uo[1] + a2 * Uo[2]
    spll_obj->osg_u[0] = (spll_obj->osg_coeff.osg_b0 * spll_obj->u[0] +
                          spll_obj->osg_coeff.osg_b2 * spll_obj->u[2] +
                          spll_obj->osg_coeff.osg_a1 * spll_obj->osg_u[1] +
                          spll_obj->osg_coeff.osg_a2 * spll_obj->osg_u[2]);
    spll_obj->osg_u[2] = spll_obj->osg_u[1];
    spll_obj->osg_u[1] = spll_obj->osg_u[0];

    //beta信号     Uqo = qb0 * u[0] + qb1 * u[1] + qb2 * u[2] + a1 * Uq[1] + a2 * Uq[2]
    spll_obj->osg_qu[0] = (spll_obj->osg_coeff.osg_qb0 * spll_obj->u[0] +
                           spll_obj->osg_coeff.osg_qb1 * spll_obj->u[1] +
                           spll_obj->osg_coeff.osg_qb2 * spll_obj->u[2] +
                           spll_obj->osg_coeff.osg_a1 * spll_obj->osg_qu[1] +
                           spll_obj->osg_coeff.osg_a2 * spll_obj->osg_qu[2]);
    spll_obj->osg_qu[2] = spll_obj->osg_qu[1];
    spll_obj->osg_qu[1] = spll_obj->osg_qu[0];

    spll_obj->u[2] = spll_obj->u[1];
    spll_obj->u[1] = spll_obj->u[0];

    //alpha信号与beta信号相互之间相差pi/2
    /*
     * 将alpha beta轴等幅值转化为dq轴
     * 计算公式如下
     * U_D = Ua * cosθ + Ub * sinθ
     * U_Q = -Ua * sinθ + Ub * cosθ
     */

    // Park 变换 (αβ → dq): 使用上一拍计算的 sinθ/cosθ
    spll_obj->u_D[0] = (spll_obj->osg_u[0] * spll_obj->cosine) + (spll_obj->osg_qu[0] * spll_obj->sine);
    spll_obj->u_Q[0] = (spll_obj->osg_qu[0] * spll_obj->cosine) - (spll_obj->osg_u[0] * spll_obj->sine);

    // PI 环路滤波器: y[k] = y[k-1] + b0 * U_Q[k] + b1 * U_Q[k-1]

    spll_obj->ylf[0] = spll_obj->ylf[1] + spll_obj->lpf_coeff.b0 * spll_obj->u_Q[0] +
                       spll_obj->lpf_coeff.b1 * spll_obj->u_Q[1];

    spll_obj->ylf[1] = spll_obj->ylf[0];
    spll_obj->u_Q[1] = spll_obj->u_Q[0];

    // VCO: 频率 = 额定偏置 + 修正量, 相位 = ∫ω dt
    spll_obj->fo = spll_obj->fn + spll_obj->ylf[0];

    spll_obj->theta += (spll_obj->fo * spll_obj->delta_t) * (float32_t)(6.2831852);

    // 相位归一化到 [0, 2π)
    if(spll_obj->theta > (float32_t)6.2831852)
        spll_obj->theta -= (float32_t)6.2831852;
    if(spll_obj->theta < 0.0f)
        spll_obj->theta += (float32_t)6.2831852;

    // 更新正余弦值（供下一拍 Park 变换使用）
    spll_obj->sine = arm_sin_f32(spll_obj->theta);
    spll_obj->cosine = arm_cos_f32(spll_obj->theta);

#if SOGI_DEBUG_DAC_OUTPUT
    HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (spll_obj->cosine + 1) * 2047);
#endif

    return spll_obj->theta;
}

void SOGI_init(void)
{
    SPLL_1PH_SOGI_reset(&spll);
    SPLL_1PH_SOGI_config(&spll, 50.0f, 20000.0f, 154.0f, -154.0f);
}
