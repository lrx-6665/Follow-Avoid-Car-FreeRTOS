#include "TIM4_PWM.h"
#include "TB6612.h"

/*
 * TB6612 电机 PWM 初始化。
 *
 * 当前函数名和实际硬件定时器已经统一为 TIM4：
 *   PB8 -> TIM4_CH3 -> PWMA，控制右侧电机速度
 *   PB9 -> TIM4_CH4 -> PWMB，控制左侧电机速度
 *
 * main.c 中调用 TIM4_PWM_Init(7199, 1) 时，PWM 频率约为 5 kHz。
 * 后续通过 TIM_SetCompare3(TIM4, value) 和 TIM_SetCompare4(TIM4, value) 修改占空比。
 */
void TIM4_PWM_Init(u16 arr,u16 psc)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	/* PB8/PB9 配置为复用推挽输出，对应 TIM4_CH3/TIM4_CH4。 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 配置 TIM4 的计数周期和分频系数，决定 PWM 频率。 */
	TIM_TimeBaseStructure.TIM_Period = arr;
	TIM_TimeBaseStructure.TIM_Prescaler = psc;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

	/* 配置 PWM 模式，初始占空比为 0，避免上电时电机突然转动。 */
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_Pulse = 0;

	TIM_OC3Init(TIM4, &TIM_OCInitStructure);
	TIM_OC4Init(TIM4, &TIM_OCInitStructure);

	TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(TIM4, ENABLE);

	TIM_Cmd(TIM4, ENABLE);
}


