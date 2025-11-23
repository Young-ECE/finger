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
#include "humidity_temp_sensor.h"
#include "microphone_sensor.h"
#include "icm42688_sensor.h"
#include <stdlib.h>
#include "methods.h"
#include "i2c_error_handler.h"


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
BME280_HandleTypeDef bme[8];  // 8x BME280 sensors via TCA9548A
ICM42688_HandleTypeDef icm42688;
MIC_HandleTypeDef mic;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void Data_Send(void)
{
  static uint32_t error_count = 0;
  static uint32_t success_count = 0;
  HAL_StatusTypeDef status;

  uint16_t als = 0, ps = 0;
  status = VCNL4040_ReadALS(&vcnl4040, &als);
  if (status != HAL_OK) {
    als = 0;
    error_count++;
  } else {
    success_count++;
  }

  status = VCNL4040_ReadPS(&vcnl4040, &ps);
  if (status != HAL_OK) {
    ps = 0;
    error_count++;
  } else {
    success_count++;
  }

  ICM42688_ScaledData accel = {0}, gyro = {0};
  float imu_temp = 0;
  status = ICM42688_ReadAll(&icm42688, &accel, &gyro, &imu_temp);
  if (status != HAL_OK) {
    accel.x = accel.y = accel.z = 0;
    gyro.x = gyro.y = gyro.z = 0;
    imu_temp = 0;
    error_count++;
  } else {
    success_count++;
  }

  float temp[8] = {0}, hum[8] = {0}, press[8] = {0};
  for (int i = 0; i < 8; i++) {
    status = BME280_ReadData(&bme[i], &temp[i], &hum[i], &press[i]);
    if (status != HAL_OK) {
      temp[i] = 0;
      hum[i] = 0;
      press[i] = 0;
      error_count++;
    } else {
      success_count++;
    }
  }

  // Decay error count on successful operations
  if (success_count > 10 && error_count > 0) {
    error_count--;
    success_count = 0;
  }

  // Conservative error recovery: only reset after many consecutive failures
  if (error_count > 200) {  // ~10 seconds @ 20Hz
    extern void I2C_BusRecover(void);
    I2C_BusRecover();
    HAL_I2C_DeInit(&hi2c1);
    HAL_Delay(10);
    MX_I2C1_Init();

    // Re-initialize sensors
    VCNL4040_Init(&vcnl4040, &hi2c1, VCNL4040_I2C_ADDR_8BIT);
    for (int i = 0; i < 8; i++) {
      BME280_Init(&bme[i], &hi2c1, TCA9548A_ADDR_70, i, BME280_ADDR_76);
    }
    ICM42688_Init(&icm42688, &hi2c1, ICM42688_ADDR_68);

    error_count = 0;
    success_count = 0;
  }

  // Send all data over USB (using rounding for better precision)
  USB_Print("%u,%u,"
            "%d,%d,%d,%d,%d,%d,%d,"
            "%d,%d,%d,%d,%d,%d,%d,%d,"
            "%d,%d,%d,%d,%d,%d,%d,%d,"
            "%d,%d,%d,%d,%d,%d,%d,%d,"
            "%ld,%ld\n",
            als, ps,
            (int)(accel.x*100 + (accel.x >= 0 ? 0.5f : -0.5f)),
            (int)(accel.y*100 + (accel.y >= 0 ? 0.5f : -0.5f)),
            (int)(accel.z*100 + (accel.z >= 0 ? 0.5f : -0.5f)),
            (int)(gyro.x*10 + (gyro.x >= 0 ? 0.5f : -0.5f)),
            (int)(gyro.y*10 + (gyro.y >= 0 ? 0.5f : -0.5f)),
            (int)(gyro.z*10 + (gyro.z >= 0 ? 0.5f : -0.5f)),
            (int)(imu_temp*10 + (imu_temp >= 0 ? 0.5f : -0.5f)),
            (int)(temp[0]*10 + (temp[0] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[0]*10 + (hum[0] >= 0 ? 0.5f : -0.5f)),
            (int)(temp[1]*10 + (temp[1] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[1]*10 + (hum[1] >= 0 ? 0.5f : -0.5f)),
            (int)(temp[2]*10 + (temp[2] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[2]*10 + (hum[2] >= 0 ? 0.5f : -0.5f)),
            (int)(temp[3]*10 + (temp[3] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[3]*10 + (hum[3] >= 0 ? 0.5f : -0.5f)),
            (int)(temp[4]*10 + (temp[4] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[4]*10 + (hum[4] >= 0 ? 0.5f : -0.5f)),
            (int)(temp[5]*10 + (temp[5] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[5]*10 + (hum[5] >= 0 ? 0.5f : -0.5f)),
            (int)(temp[6]*10 + (temp[6] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[6]*10 + (hum[6] >= 0 ? 0.5f : -0.5f)),
            (int)(temp[7]*10 + (temp[7] >= 0 ? 0.5f : -0.5f)),
            (int)(hum[7]*10 + (hum[7] >= 0 ? 0.5f : -0.5f)),
            (int)(press[0] + 0.5f), (int)(press[1] + 0.5f),
            (int)(press[2] + 0.5f), (int)(press[3] + 0.5f),
            (int)(press[4] + 0.5f), (int)(press[5] + 0.5f),
            (int)(press[6] + 0.5f), (int)(press[7] + 0.5f),
            mic.audio_left, mic.audio_right);
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

  // Fix DMA priority to avoid I2C/I2S conflict (must be called after MX_DMA_Init)
  extern void DMA_Fix_Priority(void);
  DMA_Fix_Priority();

  RGB_LED_Init();

  VCNL4040_Init(&vcnl4040, &hi2c1, VCNL4040_I2C_ADDR_8BIT);

  for (int i = 0; i < 8; i++) {
    BME280_Init(&bme[i], &hi2c1, TCA9548A_ADDR_70, i, BME280_ADDR_76);
  }

  ICM42688_Init(&icm42688, &hi2c1, ICM42688_ADDR_68);

  MIC_Init(&mic, &hi2s1);
  MIC_Start(&mic);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Data_Send();
    HAL_Delay(50);  // 20Hz update rate (reduced to prevent USB congestion)

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
