/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct __attribute__((packed)) {
  uint8_t  header;
  uint8_t  run;
  uint8_t  dir;
  uint16_t interval_ms;
  uint8_t  checksum;
} CmdPacket;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */
CmdPacket rx_packet;
volatile uint8_t packet_ready = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t  led_idx  = 0;     /* 当前亮第几颗 0/1/2 */
static uint8_t  run_flag = 1;     /* 1 运行 0 暂停 */
static uint8_t  dir_flag = 0;     /* 0 正向 1 反向 */
static uint16_t step_ms  = 500;   /* 切换间隔 */

static void LED_Show(uint8_t idx)
{
  HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, (idx == 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, (idx == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, (idx == 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 中断只记录，不处理 */
volatile uint8_t  key_dir_evt   = 0;
volatile uint8_t  key_pause_evt = 0;
static void OLED_Refresh(void)
{
  char line[17];

  snprintf(line, sizeof(line), "STATE: %-4s", run_flag ? "RUN" : "PAUS");
  OLED_ShowString(1, 1, line);

  snprintf(line, sizeof(line), "DIR:   %-4s", dir_flag ? "REV" : "FWD");
  OLED_ShowString(2, 1, line);

  snprintf(line, sizeof(line), "STEP: %04u", (unsigned)step_ms);
  OLED_ShowString(3, 1, line);

  snprintf(line, sizeof(line), "LED: %u   ", (unsigned)(led_idx + 1));
  OLED_ShowString(4, 1, line);
}
static char tx_buf[64];
static volatile uint8_t tx_busy = 0;

static void UART_SendStatus(void)
{
  if (tx_busy) return;

  int n = snprintf(tx_buf, sizeof(tx_buf),
                   "STATE=%s,DIR=%s,STEP=%u,LED=%u\r\n",
                   run_flag ? "RUN"   : "PAUSE",
                   dir_flag ? "REV"   : "FWD",
                   (unsigned)step_ms,
                   (unsigned)(led_idx + 1));

  tx_busy = 1;
  HAL_UART_Transmit_DMA(&huart1, (uint8_t *)tx_buf, (uint16_t)n);
  __HAL_DMA_DISABLE_IT(huart1.hdmatx, DMA_IT_HT);   /* 不需要"发到一半"中断 */

}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  LED_Show(led_idx);
  /* 串口测试 */
  uint8_t hello[] = "HELLO\r\n";
  HAL_UART_Transmit(&huart1, hello, sizeof(hello) - 1, 100);

  /* OLED 测试 */
  OLED_Init();
  OLED_Refresh();

  HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_packet, sizeof(rx_packet));
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_step = 0;
  while (1)
  {
    /* ---- 串口数据包 ---- */
    if (packet_ready)
    {
      CmdPacket p = rx_packet;      /* 先拷出来，避免接收新包时被改 */
      packet_ready = 0;

      const uint8_t *b = (const uint8_t *)&p;
      uint8_t expect = (uint8_t)(b[0] + b[1] + b[2] + b[3] + b[4]);

      if (p.header == 0xAA &&
          p.checksum == expect &&
          p.run <= 1 &&
          p.dir <= 1 &&
          p.interval_ms >= 100 &&
          p.interval_ms <= 2000)
      {
        run_flag = p.run;
        dir_flag = p.dir;
        step_ms  = p.interval_ms;
        OLED_Refresh();
        UART_SendStatus();
      }

      /* 必须重新启动，否则只能收一包 */
      HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_packet, sizeof(rx_packet));
    }

    /* ---- 方向键 ---- */
    if (key_dir_evt)
    {
      key_dir_evt = 0;
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET)
      {
        dir_flag = !dir_flag;
        OLED_Refresh();
        UART_SendStatus();
      }
    }

    /* ---- 暂停键 ---- */
    if (key_pause_evt)
    {
      key_pause_evt = 0;
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11) == GPIO_PIN_RESET)
      {
        run_flag = !run_flag;
        OLED_Refresh();
        UART_SendStatus();
      }
    }

    /* ---- 流水灯推进（非阻塞）---- */
    if (run_flag && (HAL_GetTick() - last_step >= step_ms))
    {
      last_step = HAL_GetTick();
      led_idx = (dir_flag == 0) ? ((led_idx + 1) % 3)
                                : ((led_idx + 2) % 3);
      LED_Show(led_idx);
      OLED_Refresh();
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED0_Pin|LED1_Pin|LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED0_Pin LED1_Pin LED2_Pin */
  GPIO_InitStruct.Pin = LED0_Pin|LED1_Pin|LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY1_Pin KEY2_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin|KEY2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_1)  { key_dir_evt   = 1; }
  if (GPIO_Pin == GPIO_PIN_11) { key_pause_evt = 1; }
}
  void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
  {
    if (huart->Instance == USART1)
    {
      packet_ready = 1;
    }
  }
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    tx_busy = 0;
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
