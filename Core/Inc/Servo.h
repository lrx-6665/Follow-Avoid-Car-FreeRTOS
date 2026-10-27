#ifndef __Servo_H
#define __Servo_H
#include "sys.h"

/*
 * SG90：PB0 -> TIM3_CH3，50Hz。
 * 正前方按实际安装校准。探头偏左就把 SERVO_FRONT_ANGLE 调小，偏右则调大。
 * 右转角度小于 0 时，Servo_SetAngle() 会夹到 0 度。
 */
#define SERVO_FRONT_ANGLE  40
#define SERVO_LEFT_ANGLE   (SERVO_FRONT_ANGLE + 45)
#define SERVO_RIGHT_ANGLE  (SERVO_FRONT_ANGLE - 45)

void myServo_Init(u16 arr, u16 psc);
void Servo_SetAngle(int angle);

#endif
