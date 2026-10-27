#include "USART3.h"
#include "sys.h"

u8 USART3_RX_BUF[USART3_REC_LEN];
u16 USART3_RX_STA=0;
/*
*** USART3  配置串口3与外部模块通信 ***

*** 波特率：9600，8位数据位，1位停止位，无奇偶校验位 ***

*** 接口：PB10->TX   PB11->RX ***


*/
void USART3_Init(uint32_t bound)
{
	GPIO_InitTypeDef GPIO_Initstructure;
	NVIC_InitTypeDef NVIC_InitStruct;
	USART_InitTypeDef USART3_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	// USART3_TX     PB10
	GPIO_Initstructure.GPIO_Mode = GPIO_Mode_AF_PP; //复用推挽输出
	GPIO_Initstructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Initstructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_Initstructure);

	// USART3_RX     PB11
	GPIO_Initstructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Initstructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_Initstructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_Initstructure);

	USART3_InitStructure.USART_BaudRate = bound;
	USART3_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //无硬件数据流控制
	USART3_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                 //收发模式
	USART3_InitStructure.USART_Parity = USART_Parity_No;                             //无奇偶校验位
	USART3_InitStructure.USART_StopBits = USART_StopBits_1;                          //一个停止位
	USART3_InitStructure.USART_WordLength = USART_WordLength_8b;                     //字长为8位数据格式
	USART_Init(USART3, &USART3_InitStructure);
	USART_Cmd(USART3, ENABLE); //使能USART3

	NVIC_InitStruct.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;

	NVIC_Init(&NVIC_InitStruct);
//	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_ClearFlag(USART3, USART_FLAG_TC);
}


u8 Res3;
void USART3_IRQHandler(void)                	//串口3中断服务程序
{


	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)  //接收中断(接收到的数据必须是0x0d 0x0a结尾)
	{
		Res3 =USART_ReceiveData(USART3);//(USART3->DR);	//读取接收到的数据

		if((USART3_RX_STA&0x8000)==0)//接收未完成
		{
			if(USART3_RX_STA&0x4000)//接收到了0x0d
			{
				if(Res3!=0x0a)USART3_RX_STA=0;//接收错误,重新开始
				else USART3_RX_STA|=0x8000;	//接收完成了
			}
			else //还没收到0X0D
			{
				if(Res3==0x0d)USART3_RX_STA|=0x4000;
				else
				{
					USART3_RX_BUF[USART3_RX_STA&0X3FFF]=Res3 ;
					USART3_RX_STA++;
					if(USART3_RX_STA>(USART3_REC_LEN-1))USART3_RX_STA=0;//接收数据错误,重新开始接收
				}
			}
		}
  }
	USART_ClearITPendingBit(USART3, USART_IT_RXNE);

}
