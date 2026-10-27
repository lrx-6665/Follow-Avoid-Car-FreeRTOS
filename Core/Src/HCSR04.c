#include "stm32f10x.h"
#include "HCSR04.h"
#include "Delay.h"
#include "car_rtos.h"

/*
 * Echo 接到 TIM2_CH2。上升沿和下降沿都由硬件锁存计数，
 * 脉宽不包含中断响应时间。TIM2 时钟 1MHz，1 个计数 = 1us。
 */
#define HCSR04_TRIG_GPIO   GPIOA
#define HCSR04_TRIG_PIN    GPIO_Pin_0
#define HCSR04_ECHO_PIN    GPIO_Pin_1

static volatile uint8_t echo_edge;
static volatile uint8_t echo_ok;
static volatile uint16_t echo_rise;
static volatile uint16_t echo_us;

static void HCSR04_SetRising(void)
{
	TIM2->CCER &= (uint16_t)~TIM_CCER_CC2E;
	TIM2->CCER &= (uint16_t)~TIM_CCER_CC2P;
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
	TIM2->CCER |= TIM_CCER_CC2E;
}

static void HCSR04_SetFalling(void)
{
	TIM2->CCER &= (uint16_t)~TIM_CCER_CC2E;
	TIM2->CCER |= TIM_CCER_CC2P;
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
	TIM2->CCER |= TIM_CCER_CC2E;
}

static void HCSR04_TIM2_Init(void)
{
	TIM_TimeBaseInitTypeDef base;
	TIM_ICInitTypeDef ic;
	NVIC_InitTypeDef nvic;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	base.TIM_Period = 0xFFFF;
	base.TIM_Prescaler = 72 - 1;
	base.TIM_ClockDivision = TIM_CKD_DIV1;
	base.TIM_CounterMode = TIM_CounterMode_Up;
	base.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &base);

	ic.TIM_Channel = TIM_Channel_2;
	ic.TIM_ICPolarity = TIM_ICPolarity_Rising;
	ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
	ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	ic.TIM_ICFilter = 0;
	TIM_ICInit(TIM2, &ic);
	TIM_Cmd(TIM2, ENABLE);

	nvic.NVIC_IRQChannel = TIM2_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = CAR_IRQ_PRIORITY;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);
}

void HCSR04_Init(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Pin = HCSR04_TRIG_PIN;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(HCSR04_TRIG_GPIO, &gpio);

	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio.GPIO_Pin = HCSR04_ECHO_PIN;
	GPIO_Init(HCSR04_TRIG_GPIO, &gpio);

	GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
	HCSR04_TIM2_Init();
	TIM_ITConfig(TIM2, TIM_IT_CC2, DISABLE);
}

void HCSR04_Cancel(void)
{
	TIM_ITConfig(TIM2, TIM_IT_CC2, DISABLE);
	echo_edge = 0;
	HCSR04_SetRising();
}

void HCSR04_ArmAndTrigger(void)
{
	HCSR04_Cancel();
	echo_ok = 0;
	echo_us = 0;
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
	NVIC_ClearPendingIRQ(TIM2_IRQn);
	TIM_ITConfig(TIM2, TIM_IT_CC2, ENABLE);

	GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
	Delay_us(2);
	GPIO_SetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
	Delay_us(20);
	GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
}

uint16_t HCSR04_DistanceCm(void)
{
	uint16_t cm;

	if(!echo_ok)
	{
		return 0;
	}

	cm = (uint16_t)((echo_us + 29U) / 58U);
	if(cm == 0 || cm > 400)
	{
		return 0;
	}
	return cm;
}

void TIM2_IRQHandler(void)
{
	BaseType_t higher = pdFALSE;

	if(TIM_GetITStatus(TIM2, TIM_IT_CC2) != RESET)
	{
		if(echo_edge == 0)
		{
			echo_rise = (uint16_t)TIM_GetCapture2(TIM2);
			echo_edge = 1;
			HCSR04_SetFalling();
		}
		else
		{
			uint16_t fall = (uint16_t)TIM_GetCapture2(TIM2);
			echo_us = (uint16_t)(fall - echo_rise);
			echo_ok = 1;
			echo_edge = 0;
			TIM_ITConfig(TIM2, TIM_IT_CC2, DISABLE);
			HCSR04_SetRising();
			if(g_control_task != NULL)
			{
				xTaskNotifyFromISR(g_control_task, CAR_NOTE_ECHO, eSetBits, &higher);
			}
		}
	}

	portYIELD_FROM_ISR(higher);
}
