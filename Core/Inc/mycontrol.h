#ifndef __mycontrol_H
#define __mycontrol_H

#include "car_rtos.h"

extern uint16_t HCSR04_M;
extern uint16_t HCSR04_L;
extern uint16_t HCSR04_R;
extern int X1, X2, X3, X4, X5;

/* ??????????????????????? */
CarMode Avoidance(void);
void Trace_task(void);

#endif
