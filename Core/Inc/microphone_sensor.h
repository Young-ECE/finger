/**
 * @file microphone_sensor.h
 * @brief ICS-43434 I2S Digital MEMS microphone interface
 * @version 1.0
 * @date 2025-10
 *
 * Reference: InvenSense ICS-43434 Datasheet (DS-000069 v1.2)
 */

#ifndef __MICROPHONE_SENSOR_H__
#define __MICROPHONE_SENSOR_H__

#include "stm32f4xx_hal.h"
#include "audio_stream_transport.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIC_BUFFER_SIZE AUDIO_STREAM_DMA_BUFFER_WORDS

typedef struct
{
    I2S_HandleTypeDef *hi2s;
    volatile int32_t audio_result_left;
    volatile int32_t audio_result_right;
    volatile uint8_t half_ready;
    volatile uint8_t full_ready;
} MIC_HandleTypeDef;

HAL_StatusTypeDef MIC_Init(MIC_HandleTypeDef *mic, I2S_HandleTypeDef *hi2s);
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic, uint16_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* __MICROPHONE_SENSOR_H__ */
