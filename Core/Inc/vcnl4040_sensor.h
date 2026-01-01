/**
 * @file vcnl4040_sensor.h
 * @brief VCNL4040 Ambient Light & Proximity Sensor Driver (I2C)
 * @version 1.0
 * @date 2025-10
 * 
 * Compatible with STM32 HAL I2C API
 */

#ifndef __VCNL4040_SENSOR_H__
#define __VCNL4040_SENSOR_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==== I2C ADDRESS ==== */
#define VCNL4040_I2C_ADDR_7BIT     (0x60)
#define VCNL4040_I2C_ADDR_8BIT     (VCNL4040_I2C_ADDR_7BIT << 1)

/* ==== REGISTER MAP (from datasheet) ==== */
#define VCNL4040_REG_ALS_CONF      0x00
#define VCNL4040_REG_ALS_THDH      0x01
#define VCNL4040_REG_ALS_THDL      0x02
#define VCNL4040_REG_PS_CONF1_2    0x03
#define VCNL4040_REG_PS_CONF3_MS   0x04
#define VCNL4040_REG_PS_CANC       0x05
#define VCNL4040_REG_PS_THDL       0x06
#define VCNL4040_REG_PS_THDH       0x07
#define VCNL4040_REG_PS_DATA       0x08
#define VCNL4040_REG_ALS_DATA      0x09
#define VCNL4040_REG_WHITE_DATA    0x0A
#define VCNL4040_REG_INT_FLAG      0x0B
#define VCNL4040_REG_DEVICE_ID     0x0C

/* ==== CONFIGURATION MACROS ==== */
#define VCNL4040_ALS_IT_80MS       (0x00 << 6)
#define VCNL4040_ALS_IT_160MS      (0x01 << 6)
#define VCNL4040_ALS_IT_320MS      (0x02 << 6)
#define VCNL4040_ALS_IT_640MS      (0x03 << 6)
#define VCNL4040_ALS_SD_OFF        (0x00)
#define VCNL4040_ALS_SD_ON         (0x01)

#define VCNL4040_LED_I_50MA        (0x00)
#define VCNL4040_LED_I_120MA       (0x03)
#define VCNL4040_LED_I_200MA       (0x07)

/* ==== STRUCTURE ==== */
typedef enum {
    VCNL_STATE_IDLE,
    VCNL_STATE_READING_ALS,
    VCNL_STATE_READING_PS
} VCNL_State_t;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t i2c_addr;
	
	// --- ?? DMA ?? ---
    uint8_t dma_buffer[2];    // DMA (Ping-Pong )
    uint16_t als_raw;         //  ALS 
    uint16_t ps_raw;          //  PS 
    volatile VCNL_State_t state; // 
} VCNL4040_HandleTypeDef;



/* ==== FUNCTION PROTOTYPES ==== */
HAL_StatusTypeDef VCNL4040_Init(VCNL4040_HandleTypeDef *dev,I2C_HandleTypeDef *hi2c,uint8_t address);
HAL_StatusTypeDef VCNL4040_ReadALS(VCNL4040_HandleTypeDef *dev, uint16_t *als);
HAL_StatusTypeDef VCNL4040_ReadPS(VCNL4040_HandleTypeDef *dev, uint16_t *ps);
HAL_StatusTypeDef VCNL4040_ReadID(VCNL4040_HandleTypeDef *dev, uint16_t *id);

HAL_StatusTypeDef VCNL4040_Start_DMA_Loop(VCNL4040_HandleTypeDef *dev);
//void VCNL4040_DMA_Callback(VCNL4040_HandleTypeDef *dev); // ?? I2C ??????

// ?????????????,???????
void VCNL4040_Parse_ALS(VCNL4040_HandleTypeDef *dev);
void VCNL4040_Parse_PS(VCNL4040_HandleTypeDef *dev);

#ifdef __cplusplus
}
#endif

#endif /* __VCNL4040_SENSOR_H__ */
