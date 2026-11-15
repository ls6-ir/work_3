# 电控组第三次培训作业

- **张俊宏**
- ***20263008258***

## 一、参考资料与驱动来源

1. 参考资料  
   GPIO 输入与内部上拉：Keysking STM32 教程第 4 集、第 5 集。  
   外部中断原理与配置：Keysking 第 6 集、第 7 集；江协科技 [5-1] EXTI 外部中断。  
   串口轮询与中断收发：Keysking 第 8 集、第 9 集；江协科技 [9-1] 至 [9-3] 串口协议与收发。  
   串口数据包解析思路：Keysking 第 11 集；江协科技 [9-4] 串口数据包、[9-5] 串口收发 HEX 与文本数据包。  
   串口 DMA 发送：Keysking 第 10 集；江协科技 [8-1]、[8-2] DMA 原理与数据转运。  
   I²C 通信与 OLED 使用：Keysking 第 12 集；江协科技 [10-1] I2C 通信协议、[4-1] 与 [4-2] OLED 显示。  
   需要说明的是，江协科技教程使用标准库编写，本工程基于 HAL 库实现，仅参考其通信协议与外设原理的讲解，未直接沿用其代码。
2. 驱动来源  
   Core/Src/OLED.c、Core/Inc/OLED.h、Core/Inc/OLED_Font.h 三个文件来自电控组群文件提供的软件 I²C OLED 驱动

## 二、定义串口反馈的格式

STATE=<RUN/PAUSE>,DIR=<FWD/REV>,STEP=<毫秒>,LED=<1~3>

STATE=<运行状态>,DIR=<方向>,STEP=<间隔>,LED=<灯号>

## 三、**选做：使用 DMA 发送串口反馈**

HAL_UART_Transmit_DMA(\&huart1, (uint8_t *)tx_buf, (uint16_t)n);

HAL_DMA_DISABLE_IT(huart1.hdmatx, DMA_IT_HT);
