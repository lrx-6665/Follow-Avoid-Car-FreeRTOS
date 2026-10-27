#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "TB6612.h"
#include "Follow.h"
#include "car_rtos.h"

#define FILTER_N          3
#define DEAD_ZONE_MM      20
#define LINK_TIMEOUT_MS   300

#define SPEED_NORMAL_L    2500
#define SPEED_NORMAL_R    2500
#define SPEED_FAST_L      7000
#define SPEED_FAST_R      7200
#define SPEED_SLOW_L      4200
#define SPEED_SLOW_R      4500

#define SENSOR_FRAME_HEAD 0xA5

typedef struct
{
	uint16_t right_mm;
	uint16_t left_mm;
} FollowRange;

static volatile uint8_t Sensor1_RxData[2];
static volatile uint8_t Sensor2_RxData[2];
static volatile uint8_t Sensor1_RxFlag;
static volatile uint8_t Sensor2_RxFlag;

static uint16_t dist1;
static uint16_t dist2;
static int8_t ok1;
static int8_t ok2;
static uint32_t sum1;
static uint32_t sum2;
static uint8_t cnt1;
static uint8_t cnt2;

static QueueHandle_t dist_queue;
static StaticQueue_t dist_queue_buf;
static uint8_t dist_queue_storage[sizeof(FollowRange)];

static TimerHandle_t link_timer;
static StaticTimer_t link_timer_buf;
static volatile TickType_t last_rx_tick;
static volatile uint8_t link_ok;

static TaskHandle_t follow_task;
static StaticTask_t follow_tcb;
static StackType_t follow_stack[256];

static void Follow_USART_Init(USART_TypeDef *usart, uint32_t baudrate)
{
	USART_InitTypeDef init;

	init.USART_BaudRate = baudrate;
	init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	init.USART_Mode = USART_Mode_Rx;
	init.USART_Parity = USART_Parity_No;
	init.USART_StopBits = USART_StopBits_1;
	init.USART_WordLength = USART_WordLength_8b;
	USART_Init(usart, &init);
	USART_Cmd(usart, ENABLE);
}

static void Follow_UpdateDistance(uint16_t raw, int8_t *valid_count,
                                  uint32_t *sum, uint8_t *count,
                                  uint16_t *distance)
{
	if(raw > 4000)
	{
		if(*valid_count > -FILTER_N)
		{
			*valid_count = (*valid_count < 0) ? (int8_t)(*valid_count - 1) : (int8_t)-1;
		}
		*sum = 0;
		*count = 0;
	}
	else
	{
		if(*valid_count < FILTER_N)
		{
			*valid_count = (*valid_count > 0) ? (int8_t)(*valid_count + 1) : (int8_t)1;
		}
		*sum += raw;
		if(*count < 255)
		{
			(*count)++;
		}
	}

	if(*valid_count <= -FILTER_N)
	{
		*distance = 0;
	}
	if(*valid_count >= FILTER_N && *count > 0)
	{
		*distance = (uint16_t)(*sum / *count);
		*sum = raw;
		*count = 1;
	}
}

static uint8_t Follow_ReadSensors(void)
{
	uint8_t frames = 0;
	uint8_t got1 = 0;
	uint8_t got2 = 0;
	uint16_t raw1 = 0;
	uint16_t raw2 = 0;

	taskENTER_CRITICAL();
	if(Sensor1_RxFlag)
	{
		raw1 = (uint16_t)(((uint16_t)Sensor1_RxData[0] << 8) | Sensor1_RxData[1]);
		Sensor1_RxFlag = 0;
		got1 = 1;
	}
	if(Sensor2_RxFlag)
	{
		raw2 = (uint16_t)(((uint16_t)Sensor2_RxData[0] << 8) | Sensor2_RxData[1]);
		Sensor2_RxFlag = 0;
		got2 = 1;
	}
	taskEXIT_CRITICAL();

	if(got1)
	{
		Follow_UpdateDistance(raw1, &ok1, &sum1, &cnt1, &dist1);
		frames++;
	}
	if(got2)
	{
		Follow_UpdateDistance(raw2, &ok2, &sum2, &cnt2, &dist2);
		frames++;
	}

	return frames;
}

static void Follow_AcceptFrame(uint8_t data, volatile uint8_t *rx,
                               volatile uint8_t *flag, uint8_t *state, uint8_t *index)
{
	if(*state == 0)
	{
		if(data == SENSOR_FRAME_HEAD && *flag == 0)
		{
			*state = 1;
			*index = 0;
		}
	}
	else
	{
		rx[(*index)++] = data;
		if(*index >= 2)
		{
			*state = 0;
			*flag = 1;
		}
	}
}

static void FollowLinkTimeout(TimerHandle_t timer)
{
	TickType_t now = xTaskGetTickCount();

	(void)timer;
	if((now - last_rx_tick) >= pdMS_TO_TICKS(LINK_TIMEOUT_MS))
	{
		link_ok = 0;
		if(g_control_task != NULL)
		{
			xTaskNotify(g_control_task, CAR_NOTE_LOST, eSetBits);
		}
	}
}

static void FollowTask(void *arg)
{
	(void)arg;

	/* 调度器已经运行，这时才允许接收中断唤醒本任务。 */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	for(;;)
	{
		FollowRange range;
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if(Follow_ReadSensors() == 0)
		{
			continue;
		}

		last_rx_tick = xTaskGetTickCount();
		link_ok = 1;
		xTimerReset(link_timer, 0);
		range.right_mm = dist1;
		range.left_mm = dist2;
		xQueueOverwrite(dist_queue, &range);
	}
}

static void Follow_HardwareInit(void)
{
	GPIO_InitTypeDef gpio;
	NVIC_InitTypeDef nvic;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio.GPIO_Pin = GPIO_Pin_10;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio);

	gpio.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOA, &gpio);

	Follow_USART_Init(USART1, 115200);
	Follow_USART_Init(USART2, 115200);

	nvic.NVIC_IRQChannelCmd = ENABLE;
	nvic.NVIC_IRQChannelPreemptionPriority = CAR_IRQ_PRIORITY;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannel = USART1_IRQn;
	NVIC_Init(&nvic);
	nvic.NVIC_IRQChannel = USART2_IRQn;
	NVIC_Init(&nvic);

	Sensor1_RxFlag = 0;
	Sensor2_RxFlag = 0;
}

void Follow_Create(void)
{
	Follow_HardwareInit();

	dist_queue = xQueueCreateStatic(1, sizeof(FollowRange),
	                                dist_queue_storage, &dist_queue_buf);
	link_timer = xTimerCreateStatic("link", pdMS_TO_TICKS(LINK_TIMEOUT_MS),
	                                pdFALSE, NULL, FollowLinkTimeout, &link_timer_buf);
	follow_task = xTaskCreateStatic(FollowTask, "follow", 256, NULL, 2,
	                                follow_stack, &follow_tcb);
	configASSERT(dist_queue != NULL);
	configASSERT(link_timer != NULL);
	configASSERT(follow_task != NULL);
}

uint8_t Follow_LinkOk(void)
{
	return link_ok;
}

void Follow_Control(void)
{
	FollowRange range;

	if(!link_ok || xQueuePeek(dist_queue, &range, 0) != pdPASS)
	{
		Set_Car_Speed(0, 0);
		return;
	}

	/* dist1 / USART1 是右侧，dist2 / USART2 是左侧。负速度为前进。 */
	if(range.right_mm == 0 && range.left_mm == 0)
	{
		Set_Car_Speed(0, 0);
	}
	else if((int32_t)range.right_mm - (int32_t)range.left_mm > DEAD_ZONE_MM)
	{
		Set_Car_Speed(-SPEED_NORMAL_L, 0);
	}
	else if((int32_t)range.left_mm - (int32_t)range.right_mm > DEAD_ZONE_MM)
	{
		Set_Car_Speed(0, -SPEED_NORMAL_R);
	}
	else
	{
		Set_Car_Speed(-SPEED_NORMAL_L, -SPEED_NORMAL_R);
	}
}

static void Follow_OnByte(USART_TypeDef *usart, volatile uint8_t *rx,
                          volatile uint8_t *flag, uint8_t *state, uint8_t *index)
{
	uint8_t data;

	if(USART_GetFlagStatus(usart, USART_FLAG_ORE) != RESET)
	{
		(void)usart->DR;
	}

	if(USART_GetITStatus(usart, USART_IT_RXNE) != RESET)
	{
		data = (uint8_t)USART_ReceiveData(usart);
		Follow_AcceptFrame(data, rx, flag, state, index);
	}
}

void USART1_IRQHandler(void)
{
	static uint8_t state;
	static uint8_t index;
	BaseType_t higher = pdFALSE;

	Follow_OnByte(USART1, Sensor1_RxData, &Sensor1_RxFlag, &state, &index);
	if(Sensor1_RxFlag && follow_task != NULL)
	{
		vTaskNotifyGiveFromISR(follow_task, &higher);
	}
	portYIELD_FROM_ISR(higher);
}

void USART2_IRQHandler(void)
{
	static uint8_t state;
	static uint8_t index;
	BaseType_t higher = pdFALSE;

	Follow_OnByte(USART2, Sensor2_RxData, &Sensor2_RxFlag, &state, &index);
	if(Sensor2_RxFlag && follow_task != NULL)
	{
		vTaskNotifyGiveFromISR(follow_task, &higher);
	}
	portYIELD_FROM_ISR(higher);
}
