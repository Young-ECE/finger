/**
 * @file vcnl4040_sensor.c
 * @brief VCNL4040 driver implementation
 */

#include "vcnl4040_sensor.h"
#include <string.h>

/* ==== INTERNAL HELPER ==== */
static HAL_StatusTypeDef VCNL4040_Write16(VCNL4040_HandleTypeDef *dev, uint8_t reg, uint8_t low, uint8_t high)
{
    uint8_t buf[2] = { low, high };
    return HAL_I2C_Mem_Write(dev->hi2c, dev->i2c_addr, reg,
                             I2C_MEMADD_SIZE_8BIT, buf, 2, 10);
}

static HAL_StatusTypeDef VCNL4040_Read16(VCNL4040_HandleTypeDef *dev, uint8_t reg, uint16_t *out)
{
    uint8_t buf[2];
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(dev->hi2c, dev->i2c_addr, reg,
                                             I2C_MEMADD_SIZE_8BIT, buf, 2, 10);
    if (ret == HAL_OK)
        *out = (uint16_t)((buf[1] << 8) | buf[0]);
    return ret;
}

/* ==== PUBLIC API ==== */

/**
 * @brief Initialize VCNL4040
 */
HAL_StatusTypeDef VCNL4040_Init(VCNL4040_HandleTypeDef *dev,I2C_HandleTypeDef *hi2c,uint8_t address)
{
    dev->i2c_addr = address;
    dev->hi2c = hi2c;

    /* Check device ID */
    uint16_t id;
    if (VCNL4040_ReadID(dev, &id) != HAL_OK || id != 0x0186)
        return HAL_ERROR;

    /* Enable ALS (160ms), power on */
    uint8_t als_conf_L = VCNL4040_ALS_IT_160MS | VCNL4040_ALS_SD_OFF;
    if (VCNL4040_Write16(dev, VCNL4040_REG_ALS_CONF, als_conf_L, 0x00) != HAL_OK)
        return HAL_ERROR;

    /* Enable PS (normal operation, LED current 120mA) */
    uint8_t ps_conf1_L = 0x00;  // PS_SD=0, duty=1/40, IT=1T
    uint8_t ps_conf1_H = 0x00;  // 16-bit mode, interrupt disable
    if (VCNL4040_Write16(dev, VCNL4040_REG_PS_CONF1_2, ps_conf1_L, ps_conf1_H) != HAL_OK)
        return HAL_ERROR;

    uint8_t ps_conf3_L = 0x00;
    uint8_t ps_conf3_H = (0x00 << 6) | VCNL4040_LED_I_120MA;
    if (VCNL4040_Write16(dev, VCNL4040_REG_PS_CONF3_MS, ps_conf3_L, ps_conf3_H) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(50);
    return HAL_OK;
}

/**
 * @brief Read ambient light (ALS)
 */
HAL_StatusTypeDef VCNL4040_ReadALS(VCNL4040_HandleTypeDef *dev, uint16_t *als)
{
    return VCNL4040_Read16(dev, VCNL4040_REG_ALS_DATA, als);
}

/**
 * @brief Read proximity sensor data (PS)
 */
HAL_StatusTypeDef VCNL4040_ReadPS(VCNL4040_HandleTypeDef *dev, uint16_t *ps)
{
    return VCNL4040_Read16(dev, VCNL4040_REG_PS_DATA, ps);
}

/**
 * @brief Read device ID (0x0186 expected)
 */
HAL_StatusTypeDef VCNL4040_ReadID(VCNL4040_HandleTypeDef *dev, uint16_t *id)
{
    return VCNL4040_Read16(dev, VCNL4040_REG_DEVICE_ID, id);
}
