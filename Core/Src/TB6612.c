#include "stm32f10x.h"
#include "Delay.h"
#include "TIM4_PWM.h"
#include "TB6612.h"

/*
 * TB6612 最终接线表。
 *
 * 右侧电机：TB6612 A 通道
 *   PWMA -> PB8 / TIM4_CH3，用 PWM 控制右侧电机速度
 *   AIN1 -> PB5，右侧电机方向控制脚 1
 *   AIN2 -> PA5，右侧电机方向控制脚 2
 *
 * 左侧电机：TB6612 B 通道
 *   PWMB -> PB9 / TIM4_CH4，用 PWM 控制左侧电机速度
 *   BIN1 -> PC15，左侧电机方向控制脚 1
 *   BIN2 -> PC14，左侧电机方向控制脚 2
 *
 * STBY -> 5V。STBY 已经硬件拉高，TB6612 上电后一直使能，程序不需要再控制 STBY。
 */
#define TB6612_AIN1_RCC    RCC_APB2Periph_GPIOB
#define TB6612_AIN1_GPIO   GPIOB
#define TB6612_AIN1_PIN    GPIO_Pin_5

#define TB6612_AIN2_RCC    RCC_APB2Periph_GPIOA
#define TB6612_AIN2_GPIO   GPIOA
#define TB6612_AIN2_PIN    GPIO_Pin_5

#define TB6612_BIN1_RCC    RCC_APB2Periph_GPIOC
#define TB6612_BIN1_GPIO   GPIOC
#define TB6612_BIN1_PIN    GPIO_Pin_15

#define TB6612_BIN2_RCC    RCC_APB2Periph_GPIOC
#define TB6612_BIN2_GPIO   GPIOC
#define TB6612_BIN2_PIN    GPIO_Pin_14

#define TB6612_PWM_MAX     7199

static void TB6612_GPIO_Out_Init(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
	if(GPIOx == GPIOC && (GPIO_Pin & (GPIO_Pin_14 | GPIO_Pin_15)))
	{
		/* PC14/PC15 驱动能力较弱，用 2MHz 更稳，控制 TB6612 输入脚已经足够。 */
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	}
	else
	{
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	}
	GPIO_Init(GPIOx, &GPIO_InitStructure);
}

static uint16_t TB6612_GetPwmValue(int speed)
{
	if(speed < 0)
	{
		speed = -speed;
	}

	if(speed > TB6612_PWM_MAX)
	{
		speed = TB6612_PWM_MAX;
	}

	return (uint16_t)speed;
}

void TB6612_Init(void)
{
	RCC_APB2PeriphClockCmd(TB6612_AIN1_RCC | TB6612_AIN2_RCC |
	                       TB6612_BIN1_RCC | TB6612_BIN2_RCC, ENABLE);

	/* 初始化 TB6612 的方向控制脚：AIN1/AIN2 控制 A 通道，BIN1/BIN2 控制 B 通道。 */
	TB6612_GPIO_Out_Init(TB6612_AIN1_GPIO, TB6612_AIN1_PIN);
	TB6612_GPIO_Out_Init(TB6612_AIN2_GPIO, TB6612_AIN2_PIN);
	TB6612_GPIO_Out_Init(TB6612_BIN1_GPIO, TB6612_BIN1_PIN);
	TB6612_GPIO_Out_Init(TB6612_BIN2_GPIO, TB6612_BIN2_PIN);

	/* STBY 已经接 5V，这里只需要先让电机保持停止状态，防止上电误动。 */
	Set_Car_Speed(0, 0);
}

/*
 * 设置左右电机速度和方向。
 * 速度范围：-7199 到 7199。
 * 正数和负数表示两个相反方向，绝对值越大，PWM 占空比越大，电机越快。
 * 左侧电机使用 B 通道：PWMB + BIN1/BIN2。
 * 右侧电机使用 A 通道：PWMA + AIN1/AIN2。
 */
void Set_Car_Speed(int Left_speed ,int Right_speed)
{
	uint16_t Left_pwm = TB6612_GetPwmValue(Left_speed);
	uint16_t Right_pwm = TB6612_GetPwmValue(Right_speed);

	/* 左侧电机：TIM4_CH4 输出到 PWMB(PB9)，PC15/PC14 接 BIN1/BIN2 控制方向。 */
	if(Left_speed > 0)
	{
		TIM_SetCompare4(TIM4, Left_pwm);
		GPIO_SetBits(TB6612_BIN1_GPIO, TB6612_BIN1_PIN);
		GPIO_ResetBits(TB6612_BIN2_GPIO, TB6612_BIN2_PIN);
	}
	else if(Left_speed < 0)
	{
		TIM_SetCompare4(TIM4, Left_pwm);
		GPIO_ResetBits(TB6612_BIN1_GPIO, TB6612_BIN1_PIN);
		GPIO_SetBits(TB6612_BIN2_GPIO, TB6612_BIN2_PIN);
	}
	else
	{
		TIM_SetCompare4(TIM4, 0);
		GPIO_ResetBits(TB6612_BIN1_GPIO, TB6612_BIN1_PIN);
		GPIO_ResetBits(TB6612_BIN2_GPIO, TB6612_BIN2_PIN);
	}

	/* 右侧电机：TIM4_CH3 输出到 PWMA(PB8)，PB5/PA5 接 AIN1/AIN2 控制方向。 */
	if(Right_speed > 0)
	{
		TIM_SetCompare3(TIM4, Right_pwm);
		GPIO_SetBits(TB6612_AIN1_GPIO, TB6612_AIN1_PIN);
		GPIO_ResetBits(TB6612_AIN2_GPIO, TB6612_AIN2_PIN);
	}
	else if(Right_speed < 0)
	{
		TIM_SetCompare3(TIM4, Right_pwm);
		GPIO_ResetBits(TB6612_AIN1_GPIO, TB6612_AIN1_PIN);
		GPIO_SetBits(TB6612_AIN2_GPIO, TB6612_AIN2_PIN);
	}
	else
	{
		TIM_SetCompare3(TIM4, 0);
		GPIO_ResetBits(TB6612_AIN1_GPIO, TB6612_AIN1_PIN);
		GPIO_ResetBits(TB6612_AIN2_GPIO, TB6612_AIN2_PIN);
	}
}





