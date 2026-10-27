#include "USART1.h"
#include "sys.h"

//u8 RxData=0x00;
u8 USART_RX_BUF[USART_REC_LEN];
u16 USART_RX_STA=0; 
/*
*** USART1  配置串口1与JDY-31通信 ***

*** 波特率：9600，8位数据位，1位停止位，无奇偶校验位 ***

*** 接口：PA9->TX   PA10->RX ***


*/
void USART1_Init(uint32_t bound)
{
	GPIO_InitTypeDef GPIO_Initstructure;
	NVIC_InitTypeDef NVIC_InitStruct;
	USART_InitTypeDef USART1_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); 

	// USART1_TX     PA9
	GPIO_Initstructure.GPIO_Mode = GPIO_Mode_AF_PP; //复用推挽输出
	GPIO_Initstructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Initstructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_Initstructure);

	// USART1_RX     PA10
	GPIO_Initstructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Initstructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Initstructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_Initstructure);

	USART1_InitStructure.USART_BaudRate = bound;
	USART1_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //无硬件数据流控制
	USART1_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                 //收模式
	USART1_InitStructure.USART_Parity = USART_Parity_No;                             //无奇偶校验位
	USART1_InitStructure.USART_StopBits = USART_StopBits_1;                          //一个停止位
	USART1_InitStructure.USART_WordLength = USART_WordLength_8b;                     //字长为8位数据格式
	USART_Init(USART1, &USART1_InitStructure);
	USART_Cmd(USART1, ENABLE); //使能USART1

	NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;

	NVIC_Init(&NVIC_InitStruct);
//	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ClearFlag(USART1, USART_FLAG_TC);
}


u8 Res;
void USART1_IRQHandler(void)                	//串口1中断服务程序
{
	

	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)  //接收中断(接收到的数据必须是0x0d 0x0a结尾)
	{
		Res =USART_ReceiveData(USART1);//(USART1->DR);	//读取接收到的数据
		
		if((USART_RX_STA&0x8000)==0)//接收未完成
		{
			if(USART_RX_STA&0x4000)//接收到了0x0d
			{
				if(Res!=0x0a)USART_RX_STA=0;//接收错误,重新开始
				else USART_RX_STA|=0x8000;	//接收完成了 
			}
			else //还没收到0X0D
			{	
				if(Res==0x0d)USART_RX_STA|=0x4000;
				else
				{
					USART_RX_BUF[USART_RX_STA&0X3FFF]=Res ;
					USART_RX_STA++;
					if(USART_RX_STA>(USART_REC_LEN-1))USART_RX_STA=0;//接收数据错误,重新开始接收	  
				}		 
			}
		}   		 
  } 
	USART_ClearITPendingBit(USART1, USART_IT_RXNE);

} 




//void USART1_IRQHandler(void)
//{
//	if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET)
//	{
//			USART_ClearFlag(USART1, USART_FLAG_RXNE);
//			RxData = USART_ReceiveData(USART1);  //接收到的数据存到 RxData

////    USART_SendData(USART1, RxData);
////    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) //等待数据发完
////            ;
//	}
//}
