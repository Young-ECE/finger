/**
 * @file i2c_dma_manager.h
 * @brief I2C DMA非阻塞读取管理器
 * 完全异步架构：I2C DMA + I2S DMA + 双缓冲零等待
 */

#ifndef __I2C_DMA_MANAGER_H
#define __I2C_DMA_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "vcnl4040_sensor.h"
#include "icm42688_sensor.h"
#include "humidity_temp_sensor.h"

/* 传感器数据缓冲区 */
typedef struct {
    // VCNL4040
    uint8_t vcnl_als_buf[2];
    uint8_t vcnl_ps_buf[2];
    
    // ICM42688
    uint8_t imu_accel_buf[6];
    uint8_t imu_gyro_buf[6];
    uint8_t imu_temp_buf[2];
    
    // BME280 x8
    uint8_t bme_data_buf[8][8];  // 每个传感器8字节数据
    
    // 完成标志
    volatile uint8_t vcnl_als_ready;
    volatile uint8_t vcnl_ps_ready;
    volatile uint8_t imu_ready;
    volatile uint8_t bme_ready[8];
    
    // 错误计数
    uint32_t error_count;
} I2C_DMA_DataTypeDef;

extern I2C_DMA_DataTypeDef i2c_dma_data;

/* 函数声明 */
void I2C_DMA_Init(void);
HAL_StatusTypeDef I2C_DMA_StartSensorReading(I2C_HandleTypeDef *hi2c);
uint8_t I2C_DMA_IsAllReady(void);
void I2C_DMA_ClearFlags(void);
void I2C_DMA_HandleError(void); /* 由i2c_error_handler调用 */

#ifdef __cplusplus
}
#endif

#endif /* __I2C_DMA_MANAGER_H */

