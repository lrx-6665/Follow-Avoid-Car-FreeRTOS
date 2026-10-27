#ifndef __HCSR04_H
#define __HCSR04_H

#include "stm32f10x.h"

/*
 * HC-SR04：PA0 = Trig，PA1 = Echo = TIM2_CH2。
 * 脉宽由输入捕获锁存，任务只负责触发和等待完成通知。
 * 返回 1~400 为厘米，0 表示这次没有有效回波。
 */
void HCSR04_Init(void);
void HCSR04_ArmAndTrigger(void);
void HCSR04_Cancel(void);
uint16_t HCSR04_DistanceCm(void);

#endif
