#ifndef G474_1_PID_H
#define G474_1_PID_H
#include <stdbool.h>
#include <stdint.h>
#include "main.h"

typedef struct {
    uint32_t adc12_buf[ADC_BUFFER_LENGTH];
    uint16_t adc3_buf[ADC_BUFFER_LENGTH];
} adc_buf_t;

/** @brief DMA半缓冲区指针对，用于队列传递 */
typedef struct {
    uint32_t *adc12_half;  /*!< 指向 adc12_buf 当前半区的指针 */
    uint16_t *adc3_half;   /*!< 指向 adc3_buf 对应半区的指针 */
} adc_dma_block_t;

void pid_ctrl_init(void);

/** @brief 设置三相线电压有效值目标，单位mV */
void pid_set_voltage(uint32_t mv);

/** @brief PG高电平表示母线电源正常 */
bool pid_is_power_good(void);

void pid_set_param(float kp, float ki, float kd);

#endif //G474_1_PID_H
