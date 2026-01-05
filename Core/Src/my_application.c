/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : my_main.c
  * @brief          : Application sensor logic implementation
  ******************************************************************************
  * @attention
  *
  * This file contains all user-defined sensor initialization and data
  * acquisition logic, separated from CubeMX-generated main.c framework.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "my_application.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "dma.h"
#include "i2c.h"
#include "i2s.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"
#include "vcnl4040_sensor.h"
#include "ens160_sensor.h"
#include "humidity_temp_sensor.h"
#include "microphone_sensor.h"
#include "icm42688_sensor.h"
#include "methods.h"
#include "i2c_error_handler.h"
#include "i2c_dma_manager.h"

/* Private variables ---------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;
extern I2S_HandleTypeDef hi2s1;
extern UART_HandleTypeDef huart1;

ICM42688_HandleTypeDef icm42688;
MIC_HandleTypeDef mic;


/* Public functions ----------------------------------------------------------*/

/**
  * @brief  Initialize all sensors and peripherals
  * @retval None
  */
void My_Application_Init(void)
{
  // ========== 传感器逐个测试 ==========
  static char msg[128];  // 声明为static避免栈溢出

  strcpy(msg, "RGB_LED_Init...\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  RGB_LED_Init();
  strcpy(msg, "RGB_LED_Init OK\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(1000);  // 等待USB枚举完成


  // Step 2: 初始化ICM42688
  strcpy(msg, "INIT:ICM42688...\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);
  ICM42688_Init(&icm42688, &hi2c1, ICM42688_ADDR_68);
  strcpy(msg, "INIT:ICM42688 OK\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);



  // Step 4: 初始化麦克风（I2S）
  strcpy(msg, "INIT:MICROPHONE...\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);
  MIC_Init(&mic, &hi2s1);
  MIC_Start(&mic);
  strcpy(msg, "INIT:MICROPHONE OK\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);

  strcpy(msg, "========== ALL SENSORS READY ==========\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(500);


}

/**
  * @brief  Main sensor reading loop (infinite)
  * @retval None (never returns)
  */
void My_Application_Run(void)
{
  // 传感器数据变量
  ICM42688_ScaledData accel = {0}, gyro = {0};
  float imu_temp = 0;

  static char msg[512];


  while (1)
  {

    // === 2. 读取ICM42688 (加速度计和陀螺仪) ===
    ICM42688_ReadAll(&icm42688, &accel, &gyro, &imu_temp);

		

    // === 4. 输出CSV格式数据 ===
    int len = sprintf(msg,
      "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"  // Accel_X/Y/Z, Gyro_X/Y/Z
      "%.2f,"  // IMU_Temp

      "%d" // Mic_Left
      "\n",  // ← 添加换行符
      accel.x, accel.y, accel.z,
      gyro.x, gyro.y, gyro.z,
      imu_temp,
      mic.audio_result_left
    );

    CDC_Transmit_FS((uint8_t*)msg, len);

		

	}
}
