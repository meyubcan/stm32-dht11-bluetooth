/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
uint8_t dhtVal[2];      /* [0] sicaklik (C), [1] nem (%) */
char    sendData[32];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define DHT_TIMEOUT_US   500U   /* hat bu kadar surede degismezse sensor yok sayilir */
#define DHT_BIT_SPLIT_US 45U    /* 26-28 us "0", yaklasik 70 us "1" */

typedef enum { DHT_PIN_INPUT = 0, DHT_PIN_OUTPUT } dht_pin_mode_t;

/* DHT11 tek hat kullandigi icin pin calisirken giris ve cikis arasinda degisir */
static void dht_set_pin_mode(dht_pin_mode_t mode)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin   = DHT11_Pin;
  GPIO_InitStruct.Mode  = (mode == DHT_PIN_OUTPUT) ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

/* Hat verilen seviyede kaldigi surece bekler, gecen sureyi mikrosaniye olarak
 * dondurur. Zaman asiminda 0 doner. TIM6 1 MHz sayiyor. */
static uint16_t dht_wait_while(GPIO_PinState level)
{
  __HAL_TIM_SET_COUNTER(&htim6, 0);
  while (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) == level) {
    if ((uint16_t)__HAL_TIM_GET_COUNTER(&htim6) > DHT_TIMEOUT_US) return 0;
  }
  uint16_t us = (uint16_t)__HAL_TIM_GET_COUNTER(&htim6);
  return us ? us : 1U;
}

/* Sensorden 40 bit okur. Basariliysa 1 doner, pData[0] sicaklik, pData[1] nem. */
uint8_t readDHT11(uint8_t *pData)
{
  uint8_t bytes[5] = {0};

  /* Baslatma: hatti en az 18 ms 0'da tut, sonra birak */
  dht_set_pin_mode(DHT_PIN_OUTPUT);
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
  HAL_Delay(20);
  dht_set_pin_mode(DHT_PIN_INPUT);

  /* Sensor cevabi: kisa bir 1, ardindan 80 us 0 ve 80 us 1 */
  if (!dht_wait_while(GPIO_PIN_SET))   return 0;
  if (!dht_wait_while(GPIO_PIN_RESET)) return 0;
  if (!dht_wait_while(GPIO_PIN_SET))   return 0;

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

  /* Her bit 50 us 0 ile baslar, ardindan gelen 1'in suresi bitin degerini verir */
  for (int i = 0; i < 40; i++) {
    if (!dht_wait_while(GPIO_PIN_RESET)) return 0;
    uint16_t high_us = dht_wait_while(GPIO_PIN_SET);
    if (!high_us) return 0;

    bytes[i / 8] <<= 1;
    if (high_us > DHT_BIT_SPLIT_US) bytes[i / 8] |= 1U;
  }

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /* Besinci bayt ilk dort baytin toplami olmali */
  if ((uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3]) != bytes[4]) return 0;

  pData[0] = bytes[2];   /* sicaklik, tam kisim */
  pData[1] = bytes[0];   /* nem, tam kisim */
  return 1;
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
  MX_TIM6_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  /* TIM6 saati 84 MHz, on bolucu 83 ile sayac tam 1 MHz olur (1 tik = 1 us) */
  __HAL_TIM_SET_PRESCALER(&htim6, 83);
  htim6.Instance->EGR = TIM_EGR_UG;
  HAL_TIM_Base_Start(&htim6);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);

    if (readDHT11(dhtVal) == 1)
    {
      /* Telefon uygulamasi "nem|sicaklik" bicimini bekliyor */
      uint16_t len = (uint16_t)sprintf(sendData, "%d|%d\n\r", dhtVal[1], dhtVal[0]);
      HAL_UART_Transmit(&huart3, (uint8_t *)sendData, len, 100);
    }
    else
    {
      uint16_t len = (uint16_t)sprintf(sendData, "DHT Read Error!!\n\r");
      HAL_UART_Transmit(&huart3, (uint8_t *)sendData, len, 100);
    }

    /* DHT11 saniyede bir defadan sik okunamaz */
    HAL_Delay(1000);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 84;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|DHT11_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LD3_Pin DHT11_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|DHT11_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
