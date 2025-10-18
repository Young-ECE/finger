/**
 * @file microphone_sensor.c
 * @brief ICS-43434 I2S Digital MEMS Microphone Implementation
 */

#include "microphone_sensor.h"
#include <string.h>


HAL_StatusTypeDef MIC_Init(MIC_HandleTypeDef *mic, I2S_HandleTypeDef *hi2s)
{
    if (!mic || !hi2s) return HAL_ERROR;
    mic->hi2s = hi2s;
    mic->half_ready = 0;
    mic->full_ready = 0;
    memset(mic->dma_buffer, 0, sizeof(mic->dma_buffer));
    memset(mic->audio_buffer, 0, sizeof(mic->audio_buffer));
    return HAL_OK;
}


/**
 * @brief 启动 DMA 采集
 */
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic)
{
    if (!mic || !mic->hi2s) return HAL_ERROR;
    return HAL_I2S_Receive_DMA(mic->hi2s, (uint16_t*)mic->dma_buffer, MIC_BUFFER_SIZE);
}


