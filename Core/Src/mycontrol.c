#include "stm32f10x.h"
#include "TB6612.h"
#include "HCSR04.h"
#include "Servo.h"
#include "mycontrol.h"

uint16_t HCSR04_M = 0;
uint16_t HCSR04_L = 0;
uint16_t HCSR04_R = 0;

#define AVOID_DISTANCE_CM     30
#define AVOID_FORWARD_SPEED   1600
#define AVOID_BACK_SPEED      1400
#define AVOID_TURN_SPEED      1500
#define HCSR04_SAMPLE_CNT     5

int X1, X2, X3, X4, X5;

static int MeasureMedian(uint16_t *out)
{
	uint16_t samples[HCSR04_SAMPLE_CNT];
	uint8_t i;
	uint8_t j;
	uint8_t valid = 0;
	uint16_t tmp;

	for(i = 0; i < HCSR04_SAMPLE_CNT; i++)
	{
		if(Car_MeasureCm(&tmp))
		{
			return 1;
		}
		if(tmp != 0)
		{
			samples[valid++] = tmp;
		}
		if(Car_WaitMs(60))
		{
			return 1;
		}
	}

	if(valid == 0)
	{
		*out = 0;
		return 0;
	}

	for(i = 0; i < valid - 1; i++)
	{
		for(j = 0; j < valid - 1 - i; j++)
		{
			if(samples[j] > samples[j + 1])
			{
				tmp = samples[j];
				samples[j] = samples[j + 1];
				samples[j + 1] = tmp;
			}
		}
	}

	*out = samples[valid / 2];
	return 0;
}

CarMode Avoidance(void)
{
	Servo_SetAngle(SERVO_FRONT_ANGLE);
	if(Car_WaitMs(500))
	{
		return Car_RequestedMode();
	}

	for(;;)
	{
		if(MeasureMedian(&HCSR04_M))
		{
			return Car_RequestedMode();
		}

		if(HCSR04_M == 0)
		{
			Set_Car_Speed(0, 0);
			if(Car_WaitMs(50))
			{
				return Car_RequestedMode();
			}
			continue;
		}

		if(HCSR04_M > AVOID_DISTANCE_CM)
		{
			Set_Car_Speed(-AVOID_FORWARD_SPEED, -AVOID_FORWARD_SPEED);
			if(Car_WaitMs(50))
			{
				return Car_RequestedMode();
			}
			continue;
		}

		Set_Car_Speed(0, 0);
		if(Car_WaitMs(200))
		{
			return Car_RequestedMode();
		}

		Servo_SetAngle(SERVO_LEFT_ANGLE);
		if(Car_WaitMs(500))
		{
			return Car_RequestedMode();
		}
		if(MeasureMedian(&HCSR04_L))
		{
			return Car_RequestedMode();
		}

		Servo_SetAngle(SERVO_RIGHT_ANGLE);
		if(Car_WaitMs(500))
		{
			return Car_RequestedMode();
		}
		if(MeasureMedian(&HCSR04_R))
		{
			return Car_RequestedMode();
		}

		Servo_SetAngle(SERVO_FRONT_ANGLE);
		if(Car_WaitMs(500))
		{
			return Car_RequestedMode();
		}

		Set_Car_Speed(AVOID_BACK_SPEED, AVOID_BACK_SPEED);
		if(Car_WaitMs(500))
		{
			return Car_RequestedMode();
		}
		Set_Car_Speed(0, 0);
		if(Car_WaitMs(200))
		{
			return Car_RequestedMode();
		}

		if(HCSR04_L >= HCSR04_R)
		{
			Set_Car_Speed(AVOID_TURN_SPEED, -AVOID_TURN_SPEED);
		}
		else
		{
			Set_Car_Speed(-AVOID_TURN_SPEED, AVOID_TURN_SPEED);
		}
		if(Car_WaitMs(1000))
		{
			return Car_RequestedMode();
		}

		Set_Car_Speed(0, 0);
		Servo_SetAngle(SERVO_FRONT_ANGLE);
		if(Car_WaitMs(500))
		{
			return Car_RequestedMode();
		}
		if(Car_WaitMs(100))
		{
			return Car_RequestedMode();
		}
	}
}

void Trace_task(void)
{
	X1 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_15);
	X2 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6);
	X3 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);
	X4 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);
	X5 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);

	if((X1 == 0 && X2 == 0 && X3 == 0 && X4 == 0 && X5 == 0) ||
	   (X1 == 1 && X2 == 1 && X3 == 1 && X4 == 1 && X5 == 1))
	{
		Set_Car_Speed(0, 0);
	}
	else if(X3 == 1)
	{
		Set_Car_Speed(2400, 2400);
	}
	else if(X2 == 1)
	{
		Set_Car_Speed(1400, -1400);
	}
	else if(X4 == 1)
	{
		Set_Car_Speed(-1400, 1400);
	}
	else if(X1 == 1)
	{
		Set_Car_Speed(1800, -1800);
	}
	else if(X5 == 1)
	{
		Set_Car_Speed(-1800, 1800);
	}
	else
	{
		Set_Car_Speed(0, 0);
	}
}
