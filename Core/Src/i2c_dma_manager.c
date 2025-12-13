/**
 * @file i2c_dma_manager.c
 * @brief I2C DMA/中断模式非阻塞读取管理器实现
 */

#include "i2c_dma_manager.h"
#include "my_application.h"
#include "humidity_temp_sensor.h"
#include <string.h>

/* 全局数据缓冲区 */
I2C_DMA_DataTypeDef i2c_dma_data = {0};

/* 外部传感器句柄（从my_main.c引用） */
extern BME280_HandleTypeDef bme[8];
extern I2C_HandleTypeDef hi2c1;

/* 前置声明 */
HAL_StatusTypeDef TCA9548A_SelectChannel(I2C_HandleTypeDef *hi2c, uint8_t mux_addr, uint8_t channel);

/* 状态机 */
typedef enum {
    SENSOR_STATE_IDLE = 0,
    SENSOR_STATE_VCNL_ALS,
    SENSOR_STATE_VCNL_PS,
    SENSOR_STATE_IMU_ACCEL,
    SENSOR_STATE_IMU_GYRO,
    SENSOR_STATE_IMU_TEMP,
    SENSOR_STATE_BME_CH0,
    SENSOR_STATE_BME_CH1,
    SENSOR_STATE_BME_CH2,
    SENSOR_STATE_BME_CH3,
    SENSOR_STATE_BME_CH4,
    SENSOR_STATE_BME_CH5,
    SENSOR_STATE_BME_CH6,
    SENSOR_STATE_BME_CH7,
    SENSOR_STATE_DONE
} SensorReadState_t;

static volatile SensorReadState_t current_state = SENSOR_STATE_IDLE;
static volatile uint8_t reading_in_progress = 0;

/**
 * @brief 初始化DMA管理器
 */
void I2C_DMA_Init(void) {
    memset(&i2c_dma_data, 0, sizeof(i2c_dma_data));
    current_state = SENSOR_STATE_IDLE;
    reading_in_progress = 0;
}

/**
 * @brief 启动所有传感器的非阻塞读取（DMA模式，状态机驱动）
 */
HAL_StatusTypeDef I2C_DMA_StartSensorReading(I2C_HandleTypeDef *hi2c) {
    if (reading_in_progress) {
        return HAL_BUSY;
    }
    
    // 清除标志
    I2C_DMA_ClearFlags();
    
    // 启动状态机
    current_state = SENSOR_STATE_VCNL_ALS;
    reading_in_progress = 1;
    
    // 启动第一个读取（VCNL4040 ALS）- 使用DMA模式
    return HAL_I2C_Mem_Read_DMA(hi2c, vcnl4040.i2c_addr, VCNL4040_REG_ALS_DATA,
                                I2C_MEMADD_SIZE_8BIT, i2c_dma_data.vcnl_als_buf, 2);
}

/**
 * @brief 检查所有传感器是否读取完成
 */
uint8_t I2C_DMA_IsAllReady(void) {
    return (current_state == SENSOR_STATE_DONE);
}

/**
 * @brief 清除就绪标志
 */
void I2C_DMA_ClearFlags(void) {
    i2c_dma_data.vcnl_als_ready = 0;
    i2c_dma_data.vcnl_ps_ready = 0;
    i2c_dma_data.imu_ready = 0;
    for (int i = 0; i < 8; i++) {
        i2c_dma_data.bme_ready[i] = 0;
    }
}

/**
 * @brief I2C Memory Read完成回调（DMA模式状态机）
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c != &hi2c1) return;
    
    HAL_StatusTypeDef status;
    
    switch (current_state) {
        case SENSOR_STATE_VCNL_ALS:
            i2c_dma_data.vcnl_als_ready = 1;
            current_state = SENSOR_STATE_VCNL_PS;
            status = HAL_I2C_Mem_Read_DMA(hi2c, vcnl4040.i2c_addr, VCNL4040_REG_PS_DATA,
                                         I2C_MEMADD_SIZE_8BIT, i2c_dma_data.vcnl_ps_buf, 2);
            if (status != HAL_OK) goto error;
            break;
            
        case SENSOR_STATE_VCNL_PS:
            i2c_dma_data.vcnl_ps_ready = 1;
            current_state = SENSOR_STATE_IMU_ACCEL;
            status = HAL_I2C_Mem_Read_DMA(hi2c, icm42688.i2c_addr, 0x1F,
                                         I2C_MEMADD_SIZE_8BIT, i2c_dma_data.imu_accel_buf, 6);
            if (status != HAL_OK) goto error;
            break;
            
        case SENSOR_STATE_IMU_ACCEL:
            current_state = SENSOR_STATE_IMU_GYRO;
            status = HAL_I2C_Mem_Read_DMA(hi2c, icm42688.i2c_addr, 0x25,
                                         I2C_MEMADD_SIZE_8BIT, i2c_dma_data.imu_gyro_buf, 6);
            if (status != HAL_OK) goto error;
            break;
            
        case SENSOR_STATE_IMU_GYRO:
            current_state = SENSOR_STATE_IMU_TEMP;
            status = HAL_I2C_Mem_Read_DMA(hi2c, icm42688.i2c_addr, 0x1D,
                                         I2C_MEMADD_SIZE_8BIT, i2c_dma_data.imu_temp_buf, 2);
            if (status != HAL_OK) goto error;
            break;
            
        case SENSOR_STATE_IMU_TEMP:
            i2c_dma_data.imu_ready = 1;
            current_state = SENSOR_STATE_BME_CH0;
            // 选择BME280通道0（这里仍需轮询模式，但很快）
            if (TCA9548A_SelectChannel(hi2c, TCA9548A_ADDR_70, 0) != HAL_OK) goto error;
            status = HAL_I2C_Mem_Read_DMA(hi2c, BME280_ADDR_76, 0xF7,
                                         I2C_MEMADD_SIZE_8BIT, i2c_dma_data.bme_data_buf[0], 8);
            if (status != HAL_OK) goto error;
            break;
            
        case SENSOR_STATE_BME_CH0:
        case SENSOR_STATE_BME_CH1:
        case SENSOR_STATE_BME_CH2:
        case SENSOR_STATE_BME_CH3:
        case SENSOR_STATE_BME_CH4:
        case SENSOR_STATE_BME_CH5:
        case SENSOR_STATE_BME_CH6:
            {
                uint8_t ch = current_state - SENSOR_STATE_BME_CH0;
                i2c_dma_data.bme_ready[ch] = 1;
                ch++;
                current_state = SENSOR_STATE_BME_CH0 + ch;
                if (ch < 8) {
                    // 继续下一个BME280
                    if (TCA9548A_SelectChannel(hi2c, TCA9548A_ADDR_70, ch) != HAL_OK) goto error;
                    status = HAL_I2C_Mem_Read_DMA(hi2c, BME280_ADDR_76, 0xF7,
                                                 I2C_MEMADD_SIZE_8BIT, i2c_dma_data.bme_data_buf[ch], 8);
                    if (status != HAL_OK) goto error;
                } else {
                    // 所有传感器读取完成
                    current_state = SENSOR_STATE_DONE;
                    reading_in_progress = 0;
                }
            }
            break;
            
        case SENSOR_STATE_BME_CH7:
            i2c_dma_data.bme_ready[7] = 1;
            current_state = SENSOR_STATE_DONE;
            reading_in_progress = 0;
            break;
            
        default:
            goto error;
    }
    
    return;
    
error:
    i2c_dma_data.error_count++;
    current_state = SENSOR_STATE_DONE;
    reading_in_progress = 0;
}

/**
 * @brief I2C状态机错误处理（由i2c_error_handler调用）
 */
void I2C_DMA_HandleError(void) {
    i2c_dma_data.error_count++;
    current_state = SENSOR_STATE_DONE;
    reading_in_progress = 0;
}

