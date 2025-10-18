/**
 * @file humidity_temp_sensor.h
 * @brief TI HDC302x (HDC3020/21/22) Temperature & Humidity Sensor Driver
 * @version 1.0
 * @date 2025-10
 *
 * Reference: TI HDC302x Datasheet (SBAS994B, Jan 2023)
 * Interface: I2C, supports 0x44/0x45/0x46/0x47
 */

#ifndef __HUMIDITY_TEMP_SENSOR_H__
#define __HUMIDITY_TEMP_SENSOR_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==== HDC302x I2C ADDRESSES ==== */
#define HDC302x_ADDR_44   (0x44 << 1)
#define HDC302x_ADDR_45   (0x45 << 1)
#define HDC302x_ADDR_46   (0x46 << 1)
#define HDC302x_ADDR_47   (0x47 << 1)

/* ==== COMMAND DEFINITIONS (Datasheet Section 8.5.1) ==== */
#define HDC302x_CMD_TRIGGER_LPM0        0x2400  // Trigger-on-demand, Low Power Mode 0
#define HDC302x_CMD_TRIGGER_LPM1        0x240B  // Trigger-on-demand, Low Power Mode 1
#define HDC302x_CMD_SOFT_RESET          0x30A2  // Software reset
#define HDC302x_CMD_HEATER_ENABLE       0x306D  // Enable heater
#define HDC302x_CMD_HEATER_DISABLE      0x3066  // Disable heater

/* ==== STRUCTURE ==== */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t i2c_addr;   // 8-bit address (already shifted)
} HDC302x_HandleTypeDef;

/* ==== FUNCTION DECLARATIONS ==== */
HAL_StatusTypeDef HDC302x_Init(HDC302x_HandleTypeDef *dev,I2C_HandleTypeDef *hi2c, uint8_t address);
HAL_StatusTypeDef HDC302x_ReadData(HDC302x_HandleTypeDef *dev, float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif /* __HUMIDITY_TEMP_SENSOR_H__ */
