#ifndef __METHODS_H__
#define __METHODS_H__

#include <stdint.h>
#include <stm32f4xx_hal.h>
// #include "usart.h"
#include "usbd_cdc_if.h"
#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

void RGB_LED_Init(void);
void RGB_LED_Blink(void);
// void USART_Print(const char *format, ...);
void I2C_Scan(void);
// void Send_Raw_Bytes(uint16_t data);
// void Send_Buffer_Bytes(uint16_t *buffer, uint16_t count);
void USB_Print(const char *format, ...);





#ifdef __cplusplus
}
#endif





#endif
