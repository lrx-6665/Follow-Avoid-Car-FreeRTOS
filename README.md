# 基于 STM32 + FreeRTOS 的智能跟随与避障小车

STM32F103C8 标准库工程。三个按键切换停止、跟随、避障。跟随用左右两路串口超声波差速，避障用 HC-SR04 加舵机扫描。任务、队列和软件定时器全部静态分配。

## 硬件

| 模块 | 接口 |
|------|------|
| MCU | STM32F103C8，72 MHz，标准外设库 |
| 电机驱动 | TB6612。右轮 PWM：PB8 / TIM4_CH3，方向 PB5、PA5；左轮 PWM：PB9 / TIM4_CH4，方向 PC15、PC14。STBY 硬件拉高 |
| 跟随测距 | 右侧 USART1 RX = PA10，左侧 USART2 RX = PA3，115200，帧头 `0xA5` + 2 字节距离（mm） |
| 避障测距 | HC-SR04，Trig = PA0，Echo = PA1 / TIM2_CH2 输入捕获，1 个计数 = 1 us |
| 舵机 | SG90，PB0 / TIM3_CH3，50 Hz。正前 40°，左右各偏 45° |
| 按键 | 跟随 PB3，停止 PB4，避障 PB10，上拉输入。关闭 JTAG，保留 SWD |
| 调试 | SWD，PA13 / PA14 |

五路灰度循迹的驱动留在 `HARDWARE/TRACE`，当前三个模式的调度循环没有调用它。

## 任务

Tick 为 1000 Hz。`configSUPPORT_DYNAMIC_ALLOCATION = 0`，任务栈单位是字。

| 任务 | 优先级 | 栈 | 作用 |
|------|--------|----|------|
| KeyTask | 4 | 128 | 10 ms 采样，连续 2 次相同才认为按键稳定，模式变化后通知控制任务 |
| ControlTask | 3 | 384 | 停止、跟随、避障。停止键可以打断测距和转向 |
| FollowTask | 2 | 256 | 解析两路串口帧，3 次有效后取平均，写入 1 深度队列 |
| 定时器任务 | 3 | 192 | 300 ms 无新帧则判定链路丢失并停车 |
| 空闲任务 | 0 | 128 | 系统空闲 |

串口和 TIM2 的抢占优先级是 6，低于 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`（5），中断里可以调用 `FromISR`。

## 行为

负的轮速表示前进。

- **跟随**：左右距离差超过 20 mm 时差速转向，否则直行。距离大于 4000 mm 视为无效。连续 3 次无效，或 300 ms 没有新帧，停车。
- **避障**：正前距离用 5 次采样取中值。大于 30 cm 前进；进入 30 cm 后停车，舵机扫左侧和右侧，倒车，再朝空间更大的一侧转向。测距等待和转向延时都能被模式通知打断。

## 编译

用 Keil MDK 打开 `源码/Project.uvprojx`，器件 STM32F103C8，Rebuild 后通过 SWD 下载。`源码/Objects` 和 `源码/Listings` 是编译输出，已忽略。

## 作者

李润欣  
GitHub：[lrx-6665](https://github.com/lrx-6665)  
邮箱：2339043687@qq.com
