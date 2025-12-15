/**
 * @file icm42688_sensor.c
 * @brief ICM42688 6-Axis IMU Driver Implementation
 *
 * Based on TDK InvenSense ICM-42688-P datasheet (DS-000347)
 */

#include "icm42688_sensor.h"
#include "methods.h"
#include <string.h>

/* ==== INTERNAL HELPER: Write Register ==== */
static HAL_StatusTypeDef ICM42688_WriteReg(ICM42688_HandleTypeDef *dev, uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg, value };
    return I2C_Protected_Master_Transmit(dev->hi2c, dev->i2c_addr, tx, 2, 50);
}

/* ==== INTERNAL HELPER: Read Register ==== */
static HAL_StatusTypeDef ICM42688_ReadReg(ICM42688_HandleTypeDef *dev, uint8_t reg, uint8_t *value)
{
    return I2C_Protected_Mem_Read(dev->hi2c, dev->i2c_addr, reg, I2C_MEMADD_SIZE_8BIT, value, 1, 50);
}

/* ==== INTERNAL HELPER: Read Multiple Registers ==== */
static HAL_StatusTypeDef ICM42688_ReadRegs(ICM42688_HandleTypeDef *dev, uint8_t reg, uint8_t *buffer, uint8_t len)
{
    /* Single attempt - let upper layer handle retries if needed */
    return I2C_Protected_Mem_Read(dev->hi2c, dev->i2c_addr, reg, I2C_MEMADD_SIZE_8BIT, buffer, len, 50);
}

/* ==== DEVICE INITIALIZATION ==== */
HAL_StatusTypeDef ICM42688_Init(ICM42688_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint8_t address)
{
    uint8_t who_am_i;

    dev->hi2c = hi2c;
    dev->i2c_addr = address;

    HAL_Delay(10); // Wait for power-up

    // Verify WHO_AM_I
    if (ICM42688_ReadReg(dev, ICM42688_REG_WHO_AM_I, &who_am_i) != HAL_OK)
        return HAL_ERROR;

    if (who_am_i != ICM42688_WHO_AM_I_VALUE)
        return HAL_ERROR;

    // Soft reset via DEVICE_CONFIG register
    if (ICM42688_WriteReg(dev, ICM42688_REG_DEVICE_CONFIG, 0x01) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(10); // Wait for reset to complete

    // Configure power management: Enable Accel and Gyro in Low Noise mode
    if (ICM42688_WriteReg(dev, ICM42688_REG_PWR_MGMT0,
                          ICM42688_PWR_GYRO_MODE_LN | ICM42688_PWR_ACCEL_MODE_LN) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(1); // Wait for mode change

    // Configure Gyro: ±2000 DPS, ODR 1kHz
    if (ICM42688_WriteReg(dev, ICM42688_REG_GYRO_CONFIG0,
                          ICM42688_GYRO_FS_2000DPS | ICM42688_ODR_1KHZ) != HAL_OK)
        return HAL_ERROR;

    // Configure Accel: ±16g, ODR 1kHz
    if (ICM42688_WriteReg(dev, ICM42688_REG_ACCEL_CONFIG0,
                          ICM42688_ACCEL_FS_16G | ICM42688_ODR_1KHZ) != HAL_OK)
        return HAL_ERROR;

    // Set scale factors based on full scale range
    // Gyro: ±2000 DPS, 16-bit -> 2000/32768 = 0.061 DPS/LSB
    dev->gyro_scale = 2000.0f / 32768.0f;

    // Accel: ±16g, 16-bit -> 16/32768 = 0.0004883 g/LSB
    dev->accel_scale = 16.0f / 32768.0f;

    return HAL_OK;
}

/* ==== READ ACCELEROMETER DATA ==== */
HAL_StatusTypeDef ICM42688_ReadAccel(ICM42688_HandleTypeDef *dev, ICM42688_ScaledData *accel)
{
    uint8_t buffer[6];
    ICM42688_RawData raw;

    if (ICM42688_ReadRegs(dev, ICM42688_REG_ACCEL_DATA_X1, buffer, 6) != HAL_OK)
        return HAL_ERROR;

    // Combine high and low bytes (big-endian)
    raw.x = (int16_t)((buffer[0] << 8) | buffer[1]);
    raw.y = (int16_t)((buffer[2] << 8) | buffer[3]);
    raw.z = (int16_t)((buffer[4] << 8) | buffer[5]);

    // Apply scale factor
    accel->x = raw.x * dev->accel_scale;
    accel->y = raw.y * dev->accel_scale;
    accel->z = raw.z * dev->accel_scale;

    return HAL_OK;
}

/* ==== READ GYROSCOPE DATA ==== */
HAL_StatusTypeDef ICM42688_ReadGyro(ICM42688_HandleTypeDef *dev, ICM42688_ScaledData *gyro)
{
    uint8_t buffer[6];
    ICM42688_RawData raw;

    if (ICM42688_ReadRegs(dev, ICM42688_REG_GYRO_DATA_X1, buffer, 6) != HAL_OK)
        return HAL_ERROR;

    // Combine high and low bytes (big-endian)
    raw.x = (int16_t)((buffer[0] << 8) | buffer[1]);
    raw.y = (int16_t)((buffer[2] << 8) | buffer[3]);
    raw.z = (int16_t)((buffer[4] << 8) | buffer[5]);

    // Apply scale factor
    gyro->x = raw.x * dev->gyro_scale;
    gyro->y = raw.y * dev->gyro_scale;
    gyro->z = raw.z * dev->gyro_scale;

    return HAL_OK;
}

/* ==== READ TEMPERATURE DATA ==== */
HAL_StatusTypeDef ICM42688_ReadTemp(ICM42688_HandleTypeDef *dev, float *temperature)
{
    uint8_t buffer[2];
    int16_t raw_temp;

    if (ICM42688_ReadRegs(dev, ICM42688_REG_TEMP_DATA1, buffer, 2) != HAL_OK)
        return HAL_ERROR;

    // Combine high and low bytes (big-endian)
    raw_temp = (int16_t)((buffer[0] << 8) | buffer[1]);

    // Convert to Celsius: Temp = (TEMP_DATA / 132.48) + 25
    *temperature = (raw_temp / 132.48f) + 25.0f;

    return HAL_OK;
}

/* ==== READ ALL SENSOR DATA ==== */
HAL_StatusTypeDef ICM42688_ReadAll(ICM42688_HandleTypeDef *dev, ICM42688_ScaledData *accel,
                                    ICM42688_ScaledData *gyro, float *temperature)
{
    uint8_t buffer[14];
    ICM42688_RawData raw_accel, raw_gyro;
    int16_t raw_temp;

    // Read all data in one transaction (TEMP + ACCEL + GYRO)
    if (ICM42688_ReadRegs(dev, ICM42688_REG_TEMP_DATA1, buffer, 14) != HAL_OK)
        return HAL_ERROR;

    // Parse temperature
    raw_temp = (int16_t)((buffer[0] << 8) | buffer[1]);
    *temperature = (raw_temp / 132.48f) + 25.0f;

    // Parse accelerometer (bytes 2-7)
    raw_accel.x = (int16_t)((buffer[2] << 8) | buffer[3]);
    raw_accel.y = (int16_t)((buffer[4] << 8) | buffer[5]);
    raw_accel.z = (int16_t)((buffer[6] << 8) | buffer[7]);

    accel->x = raw_accel.x * dev->accel_scale;
    accel->y = raw_accel.y * dev->accel_scale;
    accel->z = raw_accel.z * dev->accel_scale;

    // Parse gyroscope (bytes 8-13)
    raw_gyro.x = (int16_t)((buffer[8] << 8) | buffer[9]);
    raw_gyro.y = (int16_t)((buffer[10] << 8) | buffer[11]);
    raw_gyro.z = (int16_t)((buffer[12] << 8) | buffer[13]);

    gyro->x = raw_gyro.x * dev->gyro_scale;
    gyro->y = raw_gyro.y * dev->gyro_scale;
    gyro->z = raw_gyro.z * dev->gyro_scale;

    return HAL_OK;
}
