#include "Servo.h"

/*
 * SG90 PWM。PB0 = TIM3_CH3。
 * myServo_Init(1999, 719) 时：72MHz / 720 / 2000 = 50Hz。
 * 计数单位 10us：0 度 = 0.5ms = 50，180 度 = 2.5ms = 250。
 * 这里只改脉宽。转到到位的 500ms 由控制任务用可打断的等待完成。
 */
void myServo_Init(u16 arr, u16 psc)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	TIM_TimeBaseStructure.TIM_Period = arr;
	TIM_TimeBaseStructure.TIM_Prescaler = psc;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OC3Init(TIM3, &TIM_OCInitStructure);

	TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(TIM3, ENABLE);
	TIM_Cmd(TIM3, ENABLE);
}

#define SERVO_MIN_PULSE 50
#define SERVO_MAX_PULSE 250

void Servo_SetAngle(int angle)
{
	uint16_t pulse;

	if(angle < 0)
	{
		angle = 0;
	}
	if(angle > 180)
	{
		angle = 180;
	}

	pulse = (uint16_t)(SERVO_MIN_PULSE +
	        (uint32_t)angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE) / 180);

	TIM_SetCompare3(TIM3, pulse);
}
