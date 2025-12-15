#ifndef __METHODS_H__
#define __METHODS_H__

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stm32f4xx_hal.h>
#include "usart.h"
#include "usbd_cdc_if.h"
#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

void RGB_LED_Init(void);
void RGB_LED_Blink(void);
void USART_Print(const char *format, ...);
void I2C_Scan(void);
void Send_Raw_Bytes(uint16_t data);
void Send_Buffer_Bytes(uint16_t *buffer, uint16_t count);
void USB_Print(const char *format, ...);

/* I2C Protected Transfer - 在I2C传输期间禁用I2S DMA中断 */
HAL_StatusTypeDef I2C_Protected_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                         uint16_t MemAddress, uint16_t MemAddSize,
                                         uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef I2C_Protected_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                          uint16_t MemAddress, uint16_t MemAddSize,
                                          uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef I2C_Protected_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                                uint8_t *pData, uint16_t Size, uint32_t Timeout);





#ifdef __cplusplus
}
#endif





#endif
