#ifndef __METHODS_H__
#define __METHODS_H__
#define CPU_FREQ_MHZ (SystemCoreClock / 1000000)
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
void I2C_Diagnose_And_Recover(I2C_HandleTypeDef *hi2c, char* sensor_name, HAL_StatusTypeDef status);





// DWT ????? (??? Init ?????,?????)
void DWT_Init(void);


#ifdef __cplusplus
}
#endif





#endif
