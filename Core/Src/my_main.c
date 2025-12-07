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
#include "my_main.h"
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

VCNL4040_HandleTypeDef vcnl4040;
static BME280_HandleTypeDef bme[8];  // 8x BME280 sensors via TCA9548A
ICM42688_HandleTypeDef icm42688;
MIC_HandleTypeDef mic;

static inline void DWT_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // Enable trace
  DWT->CYCCNT = 0;                                  // Reset counter
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // Enable counter
}

/* Public functions ----------------------------------------------------------*/

/**
  * @brief  Initialize all sensors and peripherals
  * @retval None
  */
void My_Main_Init(void)
{
  // ========== 传感器逐个测试 ==========
  static char msg[128];  // 声明为static避免栈溢出

  RGB_LED_Init();
  HAL_Delay(1000);  // 等待USB枚举完成

  // // Step 1: 初始化VCNL4040
  // strcpy(msg, "INIT:VCNL4040...\n");
  // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  // HAL_Delay(100);
  // VCNL4040_Init(&vcnl4040, &hi2c1, VCNL4040_I2C_ADDR_8BIT);
  // strcpy(msg, "INIT:VCNL4040 OK\n");
  // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  // HAL_Delay(100);

  // // Step 2: 初始化ICM42688
  // strcpy(msg, "INIT:ICM42688...\n");
  // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  // HAL_Delay(100);
  // ICM42688_Init(&icm42688, &hi2c1, ICM42688_ADDR_68);
  // strcpy(msg, "INIT:ICM42688 OK\n");
  // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  // HAL_Delay(100);

  // // Step 3: 初始化8个BME280传感器（跳过损坏的[3]）
  // for (int i = 0; i < 8; i++) {
  //   if (i == 3) {
  //     int len = sprintf(msg, "INIT:BME280[%d] SKIPPED (damaged)\n", i);
  //     CDC_Transmit_FS((uint8_t*)msg, len);
  //     HAL_Delay(50);
  //     continue;
  //   }

  //   int len = sprintf(msg, "INIT:BME280[%d]...\n", i);
  //   CDC_Transmit_FS((uint8_t*)msg, len);
  //   HAL_Delay(50);

  //   BME280_Init(&bme[i], &hi2c1, TCA9548A_ADDR_70, i, BME280_ADDR_76);

  //   len = sprintf(msg, "INIT:BME280[%d] OK\n", i);
  //   CDC_Transmit_FS((uint8_t*)msg, len);
  //   HAL_Delay(50);
  // }

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

#ifdef ENABLE_PROFILING
  DWT_Init();  // Enable cycle counter for performance profiling
#endif
}

/**
  * @brief  Main sensor reading loop (infinite)
  * @retval None (never returns)
  */
void My_Main_Run(void)
{
  // uint16_t als = 0, ps = 0;
  // ICM42688_ScaledData accel = {0}, gyro = {0};
  // float imu_temp = 0;
  // float temp[8], hum[8], press[8];
  static char msg[512];

  while (1)
  {
    // // === 1. 读取VCNL4040 (光线和接近传感器) ===
    // VCNL4040_ReadALS(&vcnl4040, &als);
    // VCNL4040_ReadPS(&vcnl4040, &ps);

    // // === 2. 读取ICM42688 (加速度计和陀螺仪) ===
    // ICM42688_ReadAll(&icm42688, &accel, &gyro, &imu_temp);

    // // === 3. 读取8个BME280 (温湿度和气压) ===
    // for (int i = 0; i < 8; i++) {
    //   if (i == 3) {
    //     // BME280[3]损坏，使用常量值
    //     temp[i] = 28.0f;
    //     hum[i] = 27.0f;
    //     press[i] = 1011.0f;
    //     continue;
    //   }

    //   if (TCA9548A_SelectChannel(&hi2c1, TCA9548A_ADDR_70, i) == HAL_OK) {
    //     BME280_ReadData(&bme[i], &temp[i], &hum[i], &press[i]);
    //   } else {
    //     // I2C切换失败，使用默认值
    //     temp[i] = 0.0f;
    //     hum[i] = 0.0f;
    //     press[i] = 0.0f;
    //   }
    // }

    // // === 4. 输出CSV格式数据 ===
    // int len = sprintf(msg,
    //   "%u,%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"  // ALS,PS,Accel_X/Y/Z,Gyro_X/Y/Z
    //   "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Temp[0-7]
    //   "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Hum[0-7]
    //   "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Press[0-7]
    //   "%.1f\n",  // IMU_Temp, Mic_Audio
    //   als, ps,
    //   accel.x, accel.y, accel.z,
    //   gyro.x, gyro.y, gyro.z,
    //   temp[0], temp[1], temp[2], temp[3], temp[4], temp[5], temp[6], temp[7],
    //   hum[0], hum[1], hum[2], hum[3], hum[4], hum[5], hum[6], hum[7],
    //   press[0], press[1], press[2], press[3], press[4], press[5], press[6], press[7],
    //   imu_temp
    // );

    int len = sprintf(msg, "%ld\n", mic.audio_result);
    CDC_Transmit_FS((uint8_t*)msg, len);
  }
}
