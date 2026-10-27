#include "stm32f10x.h"

/*
 * SysTick 交给 FreeRTOS 做节拍，微秒延时改用 DWT 周期计数器。
 * 只用于几十微秒的超声波触发脉冲，任务里的毫秒等待用 vTaskDelay。
 */
#define DWT_CTRL_REG     (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT_REG   (*(volatile uint32_t *)0xE0001004UL)
#define DWT_CYCCNTENA    (1UL << 0)

static uint8_t dwt_ok;

static void Delay_ByLoop(uint32_t us)
{
	/* DWT 不可用时的保底。触发脉冲只要求不少于 10us，这里宁长勿短。 */
	volatile uint32_t loops = us * 12U;

	if(loops == 0)
	{
		loops = 1;
	}
	while(loops--)
	{
		__NOP();
	}
}

void Delay_Init(void)
{
	uint32_t before;

	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT_CYCCNT_REG = 0;
	DWT_CTRL_REG |= DWT_CYCCNTENA;
	before = DWT_CYCCNT_REG;
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	dwt_ok = (uint8_t)(DWT_CYCCNT_REG != before);
}

void Delay_us(uint32_t xus)
{
	uint32_t start;
	uint32_t ticks;

	if(!dwt_ok)
	{
		Delay_ByLoop(xus);
		return;
	}

	start = DWT_CYCCNT_REG;
	ticks = xus * (SystemCoreClock / 1000000UL);
	while((DWT_CYCCNT_REG - start) < ticks)
	{
	}
}

void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);
	}
}

void Delay_s(uint32_t xs)
{
	while(xs--)
	{
		Delay_ms(1000);
	}
}
