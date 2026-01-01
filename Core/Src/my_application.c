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

// 定义全局状态机
typedef enum {
    STATE_VCNL_ALS, // 正在读 VCNL 光照
    STATE_VCNL_PS,  // 正在读 VCNL 距离
    STATE_ICM_ALL   // 正在读 ICM IMU
} System_Sensor_State_t;

volatile System_Sensor_State_t current_state = STATE_VCNL_ALS;

VCNL4040_HandleTypeDef vcnl4040;
//static BME280_HandleTypeDef bme[8];  // 8x BME280 sensors via TCA9548A
ICM42688_HandleTypeDef icm42688;
MIC_HandleTypeDef mic;
HAL_StatusTypeDef status;

/* Public functions ----------------------------------------------------------*/

/**
  * @brief  Initialize all sensors and peripherals
  * @retval None
  */
	
// --- 核心调度回调 ---
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;

    switch (current_state)
    {
        case STATE_VCNL_ALS:
            // 【修改点】明确告诉驱动：刚才读的是 ALS，请解析 ALS！
            VCNL4040_Parse_ALS(&vcnl4040); 
            
            // 切换到 PS 读取
            current_state = STATE_VCNL_PS;
            HAL_I2C_Mem_Read_DMA(&hi2c1, vcnl4040.i2c_addr, VCNL4040_REG_PS_DATA, 1, vcnl4040.dma_buffer, 2);
            break;

        case STATE_VCNL_PS:
            // 【修改点】明确告诉驱动：刚才读的是 PS，请解析 PS！
            VCNL4040_Parse_PS(&vcnl4040);
            
            // 切换到 ICM 读取
            current_state = STATE_ICM_ALL;
            ICM42688_Start_DMA_Read(&icm42688);
            break;

        case STATE_ICM_ALL:
            // ICM 的逻辑保持不变
            ICM42688_DMA_Callback(&icm42688);
            
            current_state = STATE_VCNL_ALS;
            HAL_I2C_Mem_Read_DMA(&hi2c1, vcnl4040.i2c_addr, VCNL4040_REG_ALS_DATA, 1, vcnl4040.dma_buffer, 2);
            break;
    }
}
//void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
//{
//    if (hi2c->Instance == I2C1) {
//        // 1. 打印报错（调试用，正式版可去掉）
//        // printf("I2C Error! Code: 0x%X\n", HAL_I2C_GetError(hi2c));
//        
//        // 2. 极其重要：清除 I2C 的错误标志，复位状态
//        // 如果不复位，HAL 库会一直认为 I2C 处于 Busy 或 Error 状态
//        HAL_I2C_DeInit(hi2c);
//        HAL_I2C_Init(hi2c); 
//        
//        // 3. 强制重启状态机（起搏器）
//        // 哪怕当前是读 ICM 读到一半错了，也强制回到起点 VCNL
//        current_state = STATE_VCNL_ALS;
////        HAL_Delay(5); // 稍微喘口气，等待总线稳定
//        
//        // 重新发起第一张多米诺骨牌
//        HAL_I2C_Mem_Read_DMA(hi2c, vcnl4040.i2c_addr, VCNL4040_REG_ALS_DATA, 1, vcnl4040.dma_buffer, 2);
//    }
//}
void My_Application_Init(void)
{

  RGB_LED_Init();
	HAL_Delay(500);
	
	//初始化麦克风（I2S）

  MIC_Init(&mic, &hi2s1);
  MIC_Start(&mic);

  // 初始化传感器
  current_state = STATE_VCNL_ALS;
  VCNL4040_Init(&vcnl4040, &hi2c1, VCNL4040_I2C_ADDR_8BIT);
	ICM42688_Init(&icm42688, &hi2c1, ICM42688_ADDR_68);
	HAL_Delay(500);
	HAL_I2C_Mem_Read_DMA(&hi2c1, vcnl4040.i2c_addr, VCNL4040_REG_ALS_DATA, 1, vcnl4040.dma_buffer, 2);

//  // Step 2: 初始化ICM42688
//  strcpy(msg, "INIT:ICM42688...\n");
//  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
//  HAL_Delay(100);
//  ICM42688_Init(&icm42688, &hi2c1, ICM42688_ADDR_68);
//  strcpy(msg, "INIT:ICM42688 OK\n");
//  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
//  HAL_Delay(100);

//  // Step 3: 初始化8个BME280传感器（跳过损坏的[3]）
//  for (int i = 0; i < 8; i++) {
//    if (i == 3) {
//      int len = sprintf(msg, "INIT:BME280[%d] SKIPPED (damaged)\n", i);
//      CDC_Transmit_FS((uint8_t*)msg, len);
//      HAL_Delay(50);
//      continue;
//    }

//    int len = sprintf(msg, "INIT:BME280[%d]...\n", i);
//    CDC_Transmit_FS((uint8_t*)msg, len);
//    HAL_Delay(50);

//    BME280_Init(&bme[i], &hi2c1, TCA9548A_ADDR_70, i, BME280_ADDR_76);

//    len = sprintf(msg, "INIT:BME280[%d] OK\n", i);
//    CDC_Transmit_FS((uint8_t*)msg, len);
//    HAL_Delay(50);
//  }


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

//  // BME280数据缓存（静态变量，保留上次的值）
//  static float temp[8] = {28.0f, 28.0f, 28.0f, 28.0f, 28.0f, 28.0f, 28.0f, 28.0f};
//  static float hum[8] = {27.0f, 27.0f, 27.0f, 27.0f, 27.0f, 27.0f, 27.0f, 27.0f};
//  static float press[8] = {1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f, 1011.0f};

//  // 轮询索引（静态变量，每次循环递增）
//  static int bme_index = 0;

  static char msg[512];
    

  while (1)
  {
		
		int len = sprintf(msg,
		"%u,%u,"
    "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"
    "%d\n",		
		vcnl4040.als_raw, vcnl4040.ps_raw,
		icm42688.accel_raw.x, icm42688.accel_raw.y, icm42688.accel_raw.z,icm42688.gyro_raw.x,icm42688.gyro_raw.y,icm42688.gyro_raw.z,
		mic.audio_result_left);
		CDC_Transmit_FS((uint8_t*)msg, len);
		HAL_Delay(10);
    // === 1. 读取VCNL4040 (光线和接近传感器) ===
//    status = VCNL4040_ReadALS(&vcnl4040, &als);
//    I2C_Diagnose_And_Recover(&hi2c1, "VCNL4040_ALS", status);
//        
//    status = VCNL4040_ReadPS(&vcnl4040, &ps);
//    I2C_Diagnose_And_Recover(&hi2c1, "VCNL4040_PS", status);

//    // === 2. 读取ICM42688 (加速度计和陀螺仪) ===
//    status = ICM42688_ReadAll(&icm42688, &accel, &gyro, &imu_temp);
//    I2C_Diagnose_And_Recover(&hi2c1, "ICM42688", status);
//		
//		
//    // === 3. 轮询读取单个BME280 (温湿度和气压) ===
//    if (bme_index != 3) {
//            status = TCA9548A_SelectChannel(&hi2c1, TCA9548A_ADDR_70, bme_index);
//            if (status == HAL_OK) {
//                status = BME280_ReadData(&bme[bme_index], &temp[bme_index], &hum[bme_index], &press[bme_index]);
//                I2C_Diagnose_And_Recover(&hi2c1, "BME280_Read", status);
//            } else {
//                I2C_Diagnose_And_Recover(&hi2c1, "TCA9548_Select", status);
//            }
//        }
//    bme_index = (bme_index + 1) % 8;
//				
//				
//		start_cycles = DWT->CYCCNT;

//    // === 4. 输出CSV格式数据 ===
//    int len = sprintf(msg,
//      "%u,%u,"  // ALS, PS
//      "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"  // Accel_X/Y/Z, Gyro_X/Y/Z
//      "%.2f,"  // IMU_Temp
//      "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Temp[0-7]
//      "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Hum[0-7]
//      "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"  // Press[0-7]
//      "%d" // Mic_Left
//      "\n",  // ← 添加换行符
//      als, ps,
//      accel.x, accel.y, accel.z,
//      gyro.x, gyro.y, gyro.z,
//      imu_temp,
//      temp[0], temp[1], temp[2], temp[3], temp[4], temp[5], temp[6], temp[7],
//      hum[0], hum[1], hum[2], hum[3], hum[4], hum[5], hum[6], hum[7],
//      press[0], press[1], press[2], press[3], press[4], press[5], press[6], press[7],
//      mic.audio_result_left
//    );

//    CDC_Transmit_FS((uint8_t*)msg, len);
//		end_cycles = DWT->CYCCNT;
//		duration_us = (float)(end_cycles - start_cycles) / CPU_FREQ_MHZ;

//            // 借用 bme_index 作为一个触发，每 8 次主循环打印一次耗时
//    USB_Print("Formatting & USB Send Time: %.2f us\r\n", duration_us);
        
		

	}
}
