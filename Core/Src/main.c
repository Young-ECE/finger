/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "dma.h"
#include "i2c.h"
#include "i2s.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "vcnl4040_sensor.h"
#include "ens160_sensor.h"
#include "humidity_temp_sensor.h"
#include "microphone_sensor.h"
#include <stdlib.h>
#include "methods.h"


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

/* USER CODE BEGIN PV */
extern I2C_HandleTypeDef hi2c1;
extern I2S_HandleTypeDef hi2s1;  // 例如使用 I2S1
extern UART_HandleTypeDef huart1; 


VCNL4040_HandleTypeDef vcnl4040;
ENS160_HandleTypeDef ens160;
HDC302x_HandleTypeDef hdc1, hdc2, hdc3, hdc4;
MIC_HandleTypeDef mic;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void Data_Send(void)
{
  uint8_t aqi;
  uint16_t tvoc;
  uint16_t eco2;
  ENS160_ReadAQI(&ens160, &aqi);
  ENS160_ReadTVOC(&ens160, &tvoc);
  ENS160_ReadECO2(&ens160, &eco2);

  uint16_t als, ps;
  VCNL4040_ReadALS(&vcnl4040, &als);
  VCNL4040_ReadPS(&vcnl4040, &ps);

  float T1, H1, T2, H2, T3, H3, T4, H4;

  HDC302x_ReadData(&hdc1, &T1, &H1);
  HDC302x_ReadData(&hdc2, &T2, &H2);
  HDC302x_ReadData(&hdc3, &T3, &H3);
  HDC302x_ReadData(&hdc4, &T4, &H4);

  USB_Print("%d,%d,%d,", aqi, tvoc, eco2);
  USB_Print("%u,%u,", als, ps);
  USB_Print("%d,%d,%d,%d,%d,%d,%d,%d,", (int)(T1 * 10), (int)(H1 * 10), (int)(T2 * 10), (int)(H2 * 10), (int)(T3 * 10), (int)(H3 * 10), (int)(T4 * 10), (int)(H4 * 10));
  USB_Print("%d\n", mic.audio_result);
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_I2S1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  RGB_LED_Init();

  VCNL4040_Init(&vcnl4040, &hi2c1, VCNL4040_I2C_ADDR_8BIT);
  ENS160_Init(&ens160,&hi2c1, ENS160_I2C_ADDR_LOW);
  HDC302x_Init(&hdc1, &hi2c1, HDC302x_ADDR_44);
  HDC302x_Init(&hdc2, &hi2c1, HDC302x_ADDR_45);
  HDC302x_Init(&hdc3, &hi2c1, HDC302x_ADDR_46);
  HDC302x_Init(&hdc4, &hi2c1, HDC302x_ADDR_47);
  MIC_Init(&mic, &hi2s1);
  MIC_Start(&mic);
 
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */
    Data_Send();


    // HAL_Delay();

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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
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

/* USER CODE BEGIN 4 */


/*
    uint8_t aqi;
    uint16_t tvoc, eco2;
    ENS160_ReadAQI(&ens160, &aqi);
    ENS160_ReadTVOC(&ens160, &tvoc);
    ENS160_ReadECO2(&ens160, &eco2);

    Debug_Print("%d,%d,%d\n", aqi, tvoc, eco2);


float T1, H1, T2, H2, T3, H3, T4, H4;

        HTS_ReadData(&hts1, &T1, &H1);
        HTS_ReadData(&hts2, &T2, &H2);
        HTS_ReadData(&hts3, &T3, &H3);
        HTS_ReadData(&hts4, &T4, &H4);

        printf("[S1] T=%.2f°C RH=%.1f%% | [S2] T=%.2f°C RH=%.1f%%\r\n", T1, H1, T2, H2);
        printf("[S3] T=%.2f°C RH=%.1f%% | [S4] T=%.2f°C RH=%.1f%%\r\n", T3, H3, T4, H4);


uint16_t als, ps;
        VCNL4040_ReadALS(&vcnl4040, &als);
        VCNL4040_ReadPS(&vcnl4040, &ps);
        printf("ALS=%u, PS=%u\r\n", als, ps);
        HAL_Delay(500);


*/

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
