/**
 * @file microphone_sensor.h
 * @brief ICS-43434 I2S Digital MEMS Microphone Driver (Stereo - 2 Microphones)
 * @version 3.0
 * @date 2025-11
 *
 * Reference: TDK InvenSense ICS-43434 Datasheet
 * Configuration: Dual microphones in stereo configuration (Left + Right channels)
 * - 24-bit I2S digital output
 * - MSB-aligned in 32-bit frame (bits [31:8])
 * - SEL pin: Low = Left channel, High = Right channel
 */

#ifndef __MICROPHONE_SENSOR_H__
#define __MICROPHONE_SENSOR_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIC_BUFFER_SIZE 16  // DMA buffer size (stereo: 8 samples, 2 channels each) - increased for better interrupt tolerance
#define MIC_SAMPLE_COUNT (MIC_BUFFER_SIZE / 2)

typedef struct
{
    I2S_HandleTypeDef *hi2s;        // I2S handle
    int32_t audio_left;             // Left channel result
    int32_t audio_right;            // Right channel result
    uint8_t half_ready;             // Half buffer ready flag
    uint8_t full_ready;             // Full buffer ready flag
    // Debug fields - raw DMA data for troubleshooting
    uint32_t raw_left;              // Raw left channel data from DMA
    uint32_t raw_right;             // Raw right channel data from DMA
} MIC_HandleTypeDef;

/* Initialization and startup */
HAL_StatusTypeDef MIC_Init(MIC_HandleTypeDef *mic, I2S_HandleTypeDef *hi2s);
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic);


#ifdef __cplusplus
}
#endif

#endif /* __MICROPHONE_SENSOR_H__ */
