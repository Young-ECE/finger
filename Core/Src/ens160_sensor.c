/**
 * @file ens160_sensor.c
 * @brief ENS160 Gas Sensor Driver (I2C)
 */

#include "ens160_sensor.h"
#include <string.h>

static HAL_StatusTypeDef ENS160_Write8(ENS160_HandleTypeDef *dev, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(dev->hi2c, dev->i2c_addr, reg,
                             I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
}

static HAL_StatusTypeDef ENS160_Read8(ENS160_HandleTypeDef *dev, uint8_t reg, uint8_t *value)
{
    return HAL_I2C_Mem_Read(dev->hi2c, dev->i2c_addr, reg,
                            I2C_MEMADD_SIZE_8BIT, value, 1, 100);
}

static HAL_StatusTypeDef ENS160_Read16(ENS160_HandleTypeDef *dev, uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->hi2c, dev->i2c_addr, reg,
                                             I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
    if (ret == HAL_OK)
        *value = (uint16_t)((buf[1] << 8) | buf[0]);
    return ret;
}

/* ==== PUBLIC FUNCTIONS ==== */

/**
 * @brief Initialize ENS160
 */
HAL_StatusTypeDef ENS160_Init(ENS160_HandleTypeDef *dev,I2C_HandleTypeDef *hi2c,uint8_t address)
{

    dev->i2c_addr = address;
    dev->hi2c = hi2c;

    uint16_t id;
    if (ENS160_ReadID(dev, &id) != HAL_OK || id != 0x0160)
        return HAL_ERROR;

    /* Wake to IDLE mode */
    ENS160_SetMode(dev, ENS160_IDLE);
    HAL_Delay(50);

    /* Start STANDARD gas sensing mode */
    ENS160_SetMode(dev, ENS160_STANDARD);
    HAL_Delay(200);

    return HAL_OK;
}

/**
 * @brief Set operation mode
 */
HAL_StatusTypeDef ENS160_SetMode(ENS160_HandleTypeDef *dev, uint8_t mode)
{
    return ENS160_Write8(dev, ENS160_REG_OPMODE, mode);
}

/**
 * @brief Read Air Quality Index (1–5)
 */
HAL_StatusTypeDef ENS160_ReadAQI(ENS160_HandleTypeDef *dev, uint8_t *aqi)
{
    return ENS160_Read8(dev, ENS160_REG_DATA_AQI, aqi);
}

/**
 * @brief Read Total Volatile Organic Compounds (ppb)
 */
HAL_StatusTypeDef ENS160_ReadTVOC(ENS160_HandleTypeDef *dev, uint16_t *tvoc)
{
    return ENS160_Read16(dev, ENS160_REG_DATA_TVOC, tvoc);
}

/**
 * @brief Read equivalent CO2 (ppm)
 */
HAL_StatusTypeDef ENS160_ReadECO2(ENS160_HandleTypeDef *dev, uint16_t *eco2)
{
    return ENS160_Read16(dev, ENS160_REG_DATA_ECO2, eco2);
}

/**
 * @brief Read device status register
 */
HAL_StatusTypeDef ENS160_ReadStatus(ENS160_HandleTypeDef *dev, uint8_t *status)
{
    return ENS160_Read8(dev, ENS160_REG_DEVICE_STATUS, status);
}

/**
 * @brief Read part ID (should be 0x0160)
 */
HAL_StatusTypeDef ENS160_ReadID(ENS160_HandleTypeDef *dev, uint16_t *id)
{
    return ENS160_Read16(dev, ENS160_REG_PART_ID, id);
}
