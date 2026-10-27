#include "stm32f10x.h"
#include "Delay.h"
#include "TIM4_PWM.h"
#include "TB6612.h"
#include "HCSR04.h"
#include "Servo.h"
#include "car_rtos.h"

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	Delay_Init();
	Keys_Init();
	TIM4_PWM_Init(7199, 1);
	TB6612_Init();
	Set_Car_Speed(0, 0);
	myServo_Init(1999, 719);
	Servo_SetAngle(SERVO_FRONT_ANGLE);
	Delay_ms(500);
	HCSR04_Init();
	Car_RTOS_Start();
}
