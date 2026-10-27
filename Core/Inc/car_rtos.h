#ifndef CAR_RTOS_H
#define CAR_RTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

typedef enum
{
	CAR_MODE_STOP = 0,
	CAR_MODE_FOLLOW,
	CAR_MODE_AVOID
} CarMode;

/* 控制任务通知位。同一任务上用“或”等待，停止键能打断测距和转向。 */
#define CAR_NOTE_MODE   (1UL << 0)
#define CAR_NOTE_ECHO   (1UL << 1)
#define CAR_NOTE_LOST   (1UL << 2)

/* 分组 4 下，优先级 6 低于系统调用线 5，中断里可以调用 FromISR。 */
#define CAR_IRQ_PRIORITY  6

extern TaskHandle_t g_control_task;

void Keys_Init(void);
void Car_RTOS_Start(void);

/* 返回 1 表示按键已经换成了新模式，当前动作必须停。 */
int Car_WaitMs(uint32_t ms);
int Car_MeasureCm(uint16_t *cm);
CarMode Car_RequestedMode(void);

#endif
