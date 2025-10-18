/**
 * @file microphone_sensor.c
 * @brief ICS-43434 I2S Digital MEMS Microphone Implementation
 */

#include "microphone_sensor.h"
#include <string.h>
__attribute__((section(".bss"))) uint32_t dma_buffer[4];// 原始DMA接收缓冲
extern uint32_t  dma_buffer[MIC_BUFFER_SIZE]; 

HAL_StatusTypeDef MIC_Init(MIC_HandleTypeDef *mic, I2S_HandleTypeDef *hi2s)
{
    if (!mic || !hi2s) return HAL_ERROR;
    mic->hi2s = hi2s;
    mic->half_ready = 0;
    mic->full_ready = 0;
    mic->audio_result = 0;
    return HAL_OK;
}


/**
 * @brief 启动 DMA 采集
 */
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic)
{
    if (!mic || !mic->hi2s) return HAL_ERROR;
    return HAL_I2S_Receive_DMA(mic->hi2s, (uint16_t*)dma_buffer, MIC_BUFFER_SIZE);
}


