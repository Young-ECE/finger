/**
 * @file microphone_sensor.c
 * @brief ICS-43434 I2S Digital MEMS Microphone Implementation (Stereo)
 *
 * Dual microphone configuration:
 * - Left channel: First microphone (SEL pin = Low/GND)
 * - Right channel: Second microphone (SEL pin = High/VDD)
 * - I2S receives stereo data in interleaved format: [L, R, L, R, ...]
 * - Data format: 24-bit MSB-aligned in 32-bit frame (bits [31:8])
 */

#include "microphone_sensor.h"
#include <string.h>

// I2S 24-bit mode with 32-bit DMA: Direct 32-bit transfers
// Buffer holds 16 words = 8 stereo samples (left+right pairs)
__attribute__((section(".bss"))) uint32_t dma_buffer[MIC_BUFFER_SIZE];  // DMA receive buffer
extern uint32_t dma_buffer[MIC_BUFFER_SIZE];

HAL_StatusTypeDef MIC_Init(MIC_HandleTypeDef *mic, I2S_HandleTypeDef *hi2s)
{
    if (!mic || !hi2s) return HAL_ERROR;
    mic->hi2s = hi2s;
    mic->half_ready = 0;
    mic->full_ready = 0;
    mic->audio_left = 0;
    mic->audio_right = 0;
    mic->raw_left = 0;
    mic->raw_right = 0;
    return HAL_OK;
}


/**
 * @brief Start DMA acquisition in stereo mode
 */
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic)
{
    if (!mic || !mic->hi2s) return HAL_ERROR;
    // HAL_I2S_Receive_DMA expects size in number of uint16_t (half-words)
    // With 32-bit DMA for 24-bit I2S, size = MIC_BUFFER_SIZE * 2 halfwords
    return HAL_I2S_Receive_DMA(mic->hi2s, (uint16_t*)dma_buffer, MIC_BUFFER_SIZE * 2);
}


