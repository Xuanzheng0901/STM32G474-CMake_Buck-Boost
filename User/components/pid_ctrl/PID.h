#ifndef G474_1_PID_H
#define G474_1_PID_H
#include <stdint.h>

void pid_ctrl_init(void);

float get_voltage_value(uint8_t index);

void pid_set_voltage(uint32_t mv);

#endif //G474_1_PID_H
