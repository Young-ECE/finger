/**
 * @file microphone_sensor.h
 * @brief ICS-43434 I2S Digital MEMS Microphone Driver (Left Channel)
 * @version 1.0
 * @date 2025-10
 *
 * Reference: InvenSense ICS-43434 Datasheet (DS-000069 v1.2)
 */

#ifndef __MICROPHONE_SENSOR_H__
#define __MICROPHONE_SENSOR_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIC_BUFFER_SIZE 4   // DMA 缓冲长度
#define MIC_SAMPLE_COUNT (MIC_BUFFER_SIZE / 2)

typedef struct
{
    I2S_HandleTypeDef *hi2s;              // I2S句柄
    int32_t audio_result;
    uint8_t   half_ready;               // 半缓冲就绪标志
    uint8_t   full_ready;               // 全缓冲就绪标志
} MIC_HandleTypeDef;

/* 初始化与启动 */
HAL_StatusTypeDef MIC_Init(MIC_HandleTypeDef *mic, I2S_HandleTypeDef *hi2s);
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic);
 

#ifdef __cplusplus
}
#endif

#endif /* __MICROPHONE_SENSOR_H__ */
