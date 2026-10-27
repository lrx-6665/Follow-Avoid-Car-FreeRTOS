#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "TB6612.h"
#include "Follow.h"
#include "HCSR04.h"
#include "mycontrol.h"
#include "car_rtos.h"

#define KEY_FOLLOW_PIN  GPIO_Pin_3
#define KEY_STOP_PIN    GPIO_Pin_4
#define KEY_AVOID_PIN   GPIO_Pin_10

#define NOTE_ALL  (CAR_NOTE_MODE | CAR_NOTE_ECHO | CAR_NOTE_LOST)

TaskHandle_t g_control_task = NULL;

static volatile CarMode requested_mode = CAR_MODE_STOP;

static StaticTask_t key_tcb;
static StackType_t key_stack[128];
static StaticTask_t control_tcb;
static StackType_t control_stack[384];
static StaticTask_t idle_tcb;
static StackType_t idle_stack[128];
static StaticTask_t timer_tcb;
static StackType_t timer_stack[192];

typedef enum
{
	KEY_NONE = 0,
	KEY_STOP,
	KEY_FOLLOW,
	KEY_AVOID
} KeySample;

void Keys_Init(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	gpio.GPIO_Pin = KEY_FOLLOW_PIN | KEY_STOP_PIN | KEY_AVOID_PIN;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);
}

static KeySample KeySampleNow(void)
{
	if(GPIO_ReadInputDataBit(GPIOB, KEY_STOP_PIN) == Bit_RESET)
	{
		return KEY_STOP;
	}
	if(GPIO_ReadInputDataBit(GPIOB, KEY_FOLLOW_PIN) == Bit_RESET)
	{
		return KEY_FOLLOW;
	}
	if(GPIO_ReadInputDataBit(GPIOB, KEY_AVOID_PIN) == Bit_RESET)
	{
		return KEY_AVOID;
	}
	return KEY_NONE;
}

static void KeyTask(void *arg)
{
	TickType_t wake = xTaskGetTickCount();
	KeySample raw = KEY_NONE;
	KeySample stable = KEY_NONE;
	uint8_t same = 0;
	CarMode published = CAR_MODE_STOP;

	(void)arg;
	for(;;)
	{
		KeySample now = KeySampleNow();
		CarMode mode;

		if(now == raw)
		{
			if(same < 255)
			{
				same++;
			}
		}
		else
		{
			raw = now;
			same = 1;
		}

		if(same >= 2)
		{
			stable = raw;
		}

		if(stable == KEY_STOP)
		{
			mode = CAR_MODE_STOP;
		}
		else if(stable == KEY_FOLLOW)
		{
			mode = CAR_MODE_FOLLOW;
		}
		else if(stable == KEY_AVOID)
		{
			mode = CAR_MODE_AVOID;
		}
		else
		{
			mode = published;
		}

		if(stable != KEY_NONE && mode != published)
		{
			requested_mode = mode;
			published = mode;
			xTaskNotify(g_control_task, CAR_NOTE_MODE, eSetBits);
		}

		vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
	}
}

CarMode Car_RequestedMode(void)
{
	return requested_mode;
}

int Car_WaitMs(uint32_t ms)
{
	TickType_t start = xTaskGetTickCount();
	TickType_t budget = pdMS_TO_TICKS(ms);

	if(ms != 0 && budget == 0)
	{
		budget = 1;
	}

	for(;;)
	{
		uint32_t bits = 0;
		TickType_t elapsed = xTaskGetTickCount() - start;

		if(elapsed >= budget)
		{
			return 0;
		}
		(void)xTaskNotifyWait(0, NOTE_ALL, &bits, budget - elapsed);
		if((bits & CAR_NOTE_MODE) != 0)
		{
			return 1;
		}
	}
}

int Car_MeasureCm(uint16_t *cm)
{
	TickType_t start;
	TickType_t budget = pdMS_TO_TICKS(30);
	uint32_t bits = 0;

	if(xTaskNotifyWait(0, NOTE_ALL, &bits, 0) == pdPASS)
	{
		if((bits & CAR_NOTE_MODE) != 0)
		{
			return 1;
		}
	}

	HCSR04_ArmAndTrigger();
	start = xTaskGetTickCount();
	for(;;)
	{
		TickType_t elapsed = xTaskGetTickCount() - start;
		TickType_t left;

		if(elapsed >= budget)
		{
			HCSR04_Cancel();
			*cm = 0;
			return 0;
		}

		left = budget - elapsed;
		bits = 0;
		(void)xTaskNotifyWait(0, NOTE_ALL, &bits, left);
		if((bits & CAR_NOTE_MODE) != 0)
		{
			HCSR04_Cancel();
			return 1;
		}
		if((bits & CAR_NOTE_ECHO) != 0)
		{
			*cm = HCSR04_DistanceCm();
			return 0;
		}
	}
}

static CarMode RunStop(void)
{
	Set_Car_Speed(0, 0);
	for(;;)
	{
		uint32_t bits = 0;
		CarMode mode;

		(void)xTaskNotifyWait(0, NOTE_ALL, &bits, portMAX_DELAY);
		if((bits & CAR_NOTE_MODE) == 0)
		{
			continue;
		}
		mode = requested_mode;
		if(mode != CAR_MODE_STOP)
		{
			return mode;
		}
	}
}

static CarMode RunFollow(void)
{
	for(;;)
	{
		uint32_t bits = 0;

		(void)xTaskNotifyWait(0, NOTE_ALL, &bits, pdMS_TO_TICKS(20));
		if((bits & CAR_NOTE_MODE) != 0)
		{
			Set_Car_Speed(0, 0);
			return requested_mode;
		}
		Follow_Control();
	}
}

static void ControlTask(void *arg)
{
	CarMode mode = CAR_MODE_STOP;

	(void)arg;
	for(;;)
	{
		if(mode == CAR_MODE_FOLLOW)
		{
			mode = RunFollow();
		}
		else if(mode == CAR_MODE_AVOID)
		{
			mode = Avoidance();
			Set_Car_Speed(0, 0);
		}
		else
		{
			mode = RunStop();
		}
	}
}

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *size)
{
	*tcb = &idle_tcb;
	*stack = idle_stack;
	*size = 128;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *size)
{
	*tcb = &timer_tcb;
	*stack = timer_stack;
	*size = 192;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
	(void)task;
	(void)name;
	taskDISABLE_INTERRUPTS();
	for(;;)
	{
	}
}

void Car_RTOS_Start(void)
{
	Follow_Create();

	configASSERT(xTaskCreateStatic(KeyTask, "key", 128, NULL, 4,
	                               key_stack, &key_tcb) != NULL);
	g_control_task = xTaskCreateStatic(ControlTask, "ctrl", 384, NULL, 3,
	                                   control_stack, &control_tcb);
	configASSERT(g_control_task != NULL);

	vTaskStartScheduler();
	for(;;)
	{
	}
}
