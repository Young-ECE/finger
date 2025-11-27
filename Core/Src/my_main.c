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
// static ENS160_HandleTypeDef ens160;  // Unused for now
static BME280_HandleTypeDef bme[8];  // 8x BME280 sensors via TCA9548A
ICM42688_HandleTypeDef icm42688;
MIC_HandleTypeDef mic;

/* Unused data structures - kept for future async DMA architecture */
#if 0
/* 双缓冲区（上一次的数据） */
static struct {
  uint16_t als, ps;
  float accel_x, accel_y, accel_z;
  float gyro_x, gyro_y, gyro_z;
  float imu_temp;
  float temp[8], hum[8], press[8];
} last_sensor_data = {0};

static uint8_t first_read_done = 0;
#endif

/* Private function prototypes -----------------------------------------------*/
// Performance profiling - uncomment to enable timing analysis
// #define ENABLE_PROFILING

// Enable DWT cycle counter for profiling
static inline void DWT_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // Enable trace
  DWT->CYCCNT = 0;                                  // Reset counter
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // Enable counter
}

/* Unused function - kept for future async DMA architecture */
#if 0
static void Data_Send(void)
{
  // ======== 纯异步双缓冲架构（零等待）========

  // 检查上一次DMA读取是否完成
  if (I2C_DMA_IsAllReady()) {
    // 解析并保存到缓冲区
    last_sensor_data.als = (i2c_dma_data.vcnl_als_buf[1] << 8) | i2c_dma_data.vcnl_als_buf[0];
    last_sensor_data.ps = (i2c_dma_data.vcnl_ps_buf[1] << 8) | i2c_dma_data.vcnl_ps_buf[0];

    // ICM42688
    int16_t raw_accel_x = (i2c_dma_data.imu_accel_buf[0] << 8) | i2c_dma_data.imu_accel_buf[1];
    int16_t raw_accel_y = (i2c_dma_data.imu_accel_buf[2] << 8) | i2c_dma_data.imu_accel_buf[3];
    int16_t raw_accel_z = (i2c_dma_data.imu_accel_buf[4] << 8) | i2c_dma_data.imu_accel_buf[5];
    int16_t raw_gyro_x = (i2c_dma_data.imu_gyro_buf[0] << 8) | i2c_dma_data.imu_gyro_buf[1];
    int16_t raw_gyro_y = (i2c_dma_data.imu_gyro_buf[2] << 8) | i2c_dma_data.imu_gyro_buf[3];
    int16_t raw_gyro_z = (i2c_dma_data.imu_gyro_buf[4] << 8) | i2c_dma_data.imu_gyro_buf[5];
    int16_t raw_temp = (i2c_dma_data.imu_temp_buf[0] << 8) | i2c_dma_data.imu_temp_buf[1];

    last_sensor_data.accel_x = raw_accel_x / 16384.0f;
    last_sensor_data.accel_y = raw_accel_y / 16384.0f;
    last_sensor_data.accel_z = raw_accel_z / 16384.0f;
    last_sensor_data.gyro_x = raw_gyro_x / 131.0f;
    last_sensor_data.gyro_y = raw_gyro_y / 131.0f;
    last_sensor_data.gyro_z = raw_gyro_z / 131.0f;
    last_sensor_data.imu_temp = (raw_temp / 132.48f) + 25.0f;

    // BME280 x8
    for (int i = 0; i < 8; i++) {
      uint8_t *buf = i2c_dma_data.bme_data_buf[i];
      int32_t adc_P = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
      int32_t adc_T = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
      int32_t adc_H = (buf[6] << 8) | buf[7];

      last_sensor_data.temp[i] = adc_T / 5120.0f - 40.0f;
      last_sensor_data.hum[i] = adc_H / 1024.0f;
      last_sensor_data.press[i] = adc_P / 256.0f / 100.0f;
    }

    first_read_done = 1;

    // 立即启动下一次DMA读取（完全异步，不等待）
    I2C_DMA_StartSensorReading(&hi2c1);
  }

  // 如果还没有第一次数据，尝试启动读取但不输出
  if (!first_read_done) {
    I2C_DMA_StartSensorReading(&hi2c1);
    return;
  }

  // ======== 输出上一次的数据（简化版：只输出关键数据）========
  // 格式：als,ps,ax,ay,az,gx,gy,gz,temp,t0,h0,p0,mic_l,mic_r (14字段)

  USB_Print("%u,%u,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f,%ld,%ld\n",
            last_sensor_data.als,
            last_sensor_data.ps,
            last_sensor_data.accel_x,
            last_sensor_data.accel_y,
            last_sensor_data.accel_z,
            last_sensor_data.gyro_x,
            last_sensor_data.gyro_y,
            last_sensor_data.gyro_z,
            last_sensor_data.imu_temp,
            last_sensor_data.temp[0],    // 仅第一个BME280温度
            last_sensor_data.hum[0],     // 仅第一个BME280湿度
            last_sensor_data.press[0],   // 仅第一个BME280气压
            mic.audio_left,
            mic.audio_right);
}
#endif

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
  // ========== 读取所有传感器并输出CSV格式 ==========
  uint16_t als = 0, ps = 0;
  ICM42688_ScaledData accel = {0}, gyro = {0};
  float imu_temp = 0;
  float temp[8], hum[8], press[8];

  while (1)
  {
    // === 1. 读取VCNL4040（无延迟，I2C函数内部有超时） ===
    VCNL4040_ReadALS(&vcnl4040, &als);
    VCNL4040_ReadPS(&vcnl4040, &ps);

    // === 2. 读取ICM42688 ===
    ICM42688_ReadAll(&icm42688, &accel, &gyro, &imu_temp);

    // === 3. 读取8个BME280（跳过损坏的BME280[3]） ===
    for (int i = 0; i < 8; i++) {
      if (i == 3) {
        // BME280[3]损坏，使用常量值
        temp[i] = 28.0f;
        hum[i] = 27.0f;
        press[i] = 1011.0f;
        continue;
      }

      if (TCA9548A_SelectChannel(&hi2c1, TCA9548A_ADDR_70, i) == HAL_OK) {
        BME280_ReadData(&bme[i], &temp[i], &hum[i], &press[i]);
      } else {
        temp[i] = 0; hum[i] = 0; press[i] = 0;
      }
    }

    // === 4. 合并成CSV格式输出 (分2段发送，避免buffer溢出) ===
    // 段1: 光学 + IMU + 温度
    static char msg[128];
    int len = sprintf(msg, "%u,%u,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,",
                      als, ps,
                      accel.x, accel.y, accel.z,
                      gyro.x, gyro.y, gyro.z,
                      imu_temp,
                      temp[0], temp[1], temp[2], temp[3],
                      temp[4], temp[5], temp[6], temp[7]);
    CDC_Transmit_FS((uint8_t*)msg, len);
    HAL_Delay(3);  // 3ms足够USB发送完成

    // 段2: 湿度 + 气压
    len = sprintf(msg, "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
                  hum[0], hum[1], hum[2], hum[3],
                  hum[4], hum[5], hum[6], hum[7],
                  press[0], press[1], press[2], press[3],
                  press[4], press[5], press[6], press[7]);
    CDC_Transmit_FS((uint8_t*)msg, len);

    // 无额外延迟，立即进入下一轮读取（实际周期由传感器I2C读取时间决定）
  }
}

/* ===== LEGACY CODE (PRESERVED FOR REFERENCE) ===== */
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
