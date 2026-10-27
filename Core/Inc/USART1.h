#ifndef __USART1_H
#define __USART1_H	 
#include "sys.h"

#define USART_REC_LEN  			200  	//定义最大接收字节数 200
#define EN_USART1_RX 			1		//使能（1）/禁止（0）串口1接收
extern u8  USART_RX_BUF[USART_REC_LEN]; 
extern u16 USART_RX_STA; 
void USART1_Init(uint32_t bound);	
	
#endif

