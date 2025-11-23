/**
 * @file microphone_sensor.c
 * @brief ICS-43434 I2S Digital MEMS Microphone Implementation
 */

#include "microphone_sensor.h"
#include <string.h>

// DMA缓冲区：放在.bss段，确保地址对齐
__attribute__((section(".bss"))) __attribute__((aligned(4))) 
uint32_t dma_buffer[MIC_BUFFER_SIZE]; 

HAL_StatusTypeDef MIC_Init(MIC_HandleTypeDef *mic, I2S_HandleTypeDef *hi2s)
{
    if (!mic || !hi2s) return HAL_ERROR;
    mic->hi2s = hi2s;
    mic->half_ready = 0;
    mic->full_ready = 0;
    mic->audio_left = 0;
    mic->audio_right = 0;
    return HAL_OK;
}


/**
 * @brief 启动 DMA 采集
 */
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic)
{
    if (!mic || !mic->hi2s) return HAL_ERROR;
    
    // 对于24位I2S（32位帧），DMA配置为WORD对齐
    // HAL_I2S_Receive_DMA的Size参数以半字为单位
    // MIC_BUFFER_SIZE=4 (uint32_t) = 8个半字
    return HAL_I2S_Receive_DMA(mic->hi2s, (uint16_t*)dma_buffer, MIC_BUFFER_SIZE * 2);
}


