/**
 * @file ens160_sensor.h
 * @brief ENS160 Digital Gas Sensor (TVOC, eCO2, AQI)
 * @version 1.0
 * @date 2025-10
 *
 * Supports STM32 HAL I2C driver
 */

#ifndef __ENS160_SENSOR_H__
#define __ENS160_SENSOR_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==== I2C ADDRESS ==== */
#define ENS160_I2C_ADDR_LOW   (0x52 << 1)  // ADDR pin low
#define ENS160_I2C_ADDR_HIGH  (0x53 << 1)  // ADDR pin high

/* ==== REGISTER MAP ==== */
#define ENS160_REG_PART_ID        0x00
#define ENS160_REG_OPMODE         0x10
#define ENS160_REG_CONFIG         0x11
#define ENS160_REG_COMMAND        0x12
#define ENS160_REG_TEMP_IN        0x13
#define ENS160_REG_RH_IN          0x15
#define ENS160_REG_DEVICE_STATUS  0x20
#define ENS160_REG_DATA_AQI       0x21
#define ENS160_REG_DATA_TVOC      0x22
#define ENS160_REG_DATA_ECO2      0x24
#define ENS160_REG_DATA_T         0x30
#define ENS160_REG_DATA_RH        0x32
#define ENS160_REG_DATA_MISR      0x38

/* ==== OPMODE VALUES ==== */
#define ENS160_DEEP_SLEEP   0x00
#define ENS160_IDLE         0x01
#define ENS160_STANDARD     0x02
#define ENS160_RESET        0xF0

/* ==== COMMANDS ==== */
#define ENS160_CMD_NOP          0x00
#define ENS160_CMD_GET_APPVER   0x0E
#define ENS160_CMD_CLRGPR       0xCC

/* ==== STATUS FLAGS ==== */
#define ENS160_STATUS_NEWDAT    0x02  // New data available
#define ENS160_STATUS_STATAS    0x80  // Operating mode running

/* ==== VALIDITY FLAGS ==== */
#define ENS160_VALIDITY_NORMAL      0  // Normal operation
#define ENS160_VALIDITY_WARMUP      1  // Warm-up phase (first 3 minutes)
#define ENS160_VALIDITY_STARTUP     2  // Initial start-up (first 1 hour)
#define ENS160_VALIDITY_INVALID     3  // Invalid output

/* ==== STRUCT ==== */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t i2c_addr;
} ENS160_HandleTypeDef;

/* ==== FUNCTION DECLARATIONS ==== */
HAL_StatusTypeDef ENS160_Init(ENS160_HandleTypeDef *dev,I2C_HandleTypeDef *hi2c, uint8_t address);
HAL_StatusTypeDef ENS160_SetMode(ENS160_HandleTypeDef *dev, uint8_t mode);
HAL_StatusTypeDef ENS160_ReadAQI(ENS160_HandleTypeDef *dev, uint8_t *aqi);
HAL_StatusTypeDef ENS160_ReadTVOC(ENS160_HandleTypeDef *dev, uint16_t *tvoc);
HAL_StatusTypeDef ENS160_ReadECO2(ENS160_HandleTypeDef *dev, uint16_t *eco2);
HAL_StatusTypeDef ENS160_ReadStatus(ENS160_HandleTypeDef *dev, uint8_t *status);
HAL_StatusTypeDef ENS160_ReadID(ENS160_HandleTypeDef *dev, uint16_t *id);
uint8_t ENS160_GetValidity(ENS160_HandleTypeDef *dev);
uint8_t ENS160_IsDataReady(ENS160_HandleTypeDef *dev);

#ifdef __cplusplus
}
#endif

#endif /* __ENS160_SENSOR_H__ */
