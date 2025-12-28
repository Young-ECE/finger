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

  // Step 1: 初始化VCNL4040
  strcpy(msg, "INIT:VCNL4040...\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);
  VCNL4040_Init(&vcnl4040, &hi2c1, VCNL4040_I2C_ADDR_8BIT);
  strcpy(msg, "INIT:VCNL4040 OK\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);

  // Step 2: 初始化ICM42688
  strcpy(msg, "INIT:ICM42688...\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);
  ICM42688_Init(&icm42688, &hi2c1, ICM42688_ADDR_68);
  strcpy(msg, "INIT:ICM42688 OK\n");
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  HAL_Delay(100);

  // Step 3: 初始化8个BME280传感器（跳过损坏的[3]）
  for (int i = 0; i < 8; i++) {
    if (i == 3) {
      int len = sprintf(msg, "INIT:BME280[%d] SKIPPED (damaged)\n", i);
      CDC_Transmit_FS((uint8_t*)msg, len);
      HAL_Delay(50);
      continue;
    }

    int len = sprintf(msg, "INIT:BME280[%d]...\n", i);
    CDC_Transmit_FS((uint8_t*)msg, len);
    HAL_Delay(50);

    BME280_Init(&bme[i], &hi2c1, TCA9548A_ADDR_70, i, BME280_ADDR_76);

    len = sprintf(msg, "INIT:BME280[%d] OK\n", i);
    CDC_Transmit_FS((uint8_t*)msg, len);
    HAL_Delay(50);
  }

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
void My_Application_Run(void)
{
  // // 传感器数据变量
  uint16_t als = 0, ps = 0;
  ICM42688_ScaledData accel = {0}, gyro = {0};
  float imu_temp = 0;

  // BME280数据缓存（静态变量，保留上次的值）
  static float temp[8] = {28.0f, 28.0f, 28.0f, 28.0f, 28.0f, 28.0f, 28.0f, 28.0f};
  static float hum[8] = {27.0f, 27.0f, 27.0f, 27.0f, 27.0f, 27.0f, 27.0f, 27.0f};
  static float press[8] = {1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f};

  // 轮询索引（静态变量，每次循环递增）
  static int bme_index = 0;

  static char msg[512];

  while (1)
  {
    // // === 1. 读取VCNL4040 (光线和接近传感器) ===
    VCNL4040_ReadALS(&vcnl4040, &als);
    VCNL4040_ReadPS(&vcnl4040, &ps);

    // === 2. 读取ICM42688 (加速度计和陀螺仪) ===
    ICM42688_ReadAll(&icm42688, &accel, &gyro, &imu_temp);

    // === 3. 轮询读取单个BME280 (温湿度和气压) ===
    if (bme_index == 3) {
      // BME280[3]损坏，使用常量值（已在初始化时设置）
      // 跳过读取，但仍然递增索引
    } else {
      // 读取当前索引的BME280传感器
      if (TCA9548A_SelectChannel(&hi2c1, TCA9548A_ADDR_70, bme_index) == HAL_OK) {
        BME280_ReadData(&bme[bme_index], &temp[bme_index], &hum[bme_index], &press[bme_index]);
      }
      // I2C切换失败时保留旧值，不更新
    }
    // 切换到下一个传感器（循环0-7）
    bme_index = (bme_index + 1) % 8;

    // === 4. 输出CSV格式数据 ===
    int len = sprintf(msg,
      "%u,%u,"  // ALS, PS
      "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"  // Accel_X/Y/Z, Gyro_X/Y/Z
      "%.2f,"  // IMU_Temp
      "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Temp[0-7]
      "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Hum[0-7]
      "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f"  // Press[0-7]
      ",%ld"  // Audio_Result_Left
      "\n",  // ← 添加换行符
      als, ps,
      accel.x, accel.y, accel.z,
      gyro.x, gyro.y, gyro.z,
      imu_temp,
      temp[0], temp[1], temp[2], temp[3], temp[4], temp[5], temp[6], temp[7],
      hum[0], hum[1], hum[2], hum[3], hum[4], hum[5], hum[6], hum[7],
      press[0], press[1], press[2], press[3], press[4], press[5], press[6], press[7],
      mic.audio_result_left
    );

    CDC_Transmit_FS((uint8_t*)msg, len);
    HAL_Delay(1);  // 短暂延迟，避免USB传输过快

      // if (mic.full_ready)
      // {
      //   char mic_msg[64];
      //   int len = sprintf(mic_msg, "%ld\n", mic.audio_result_left);
      //   CDC_Transmit_FS((uint8_t *)mic_msg, len);
      //   mic.full_ready = 0;
      // }
    
  }
}
