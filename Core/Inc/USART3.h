#ifndef __USART3_H
#define __USART3_H
#include "sys.h"

#define USART3_REC_LEN  		200  	//定义最大接收字节数 200
#define EN_USART3_RX 			1		//使能（1）/禁止（0）串口3接收
extern u8  USART3_RX_BUF[USART3_REC_LEN];
extern u16 USART3_RX_STA;
void USART3_Init(uint32_t bound);

#endif
