/**
 * @file humidity_temp_sensor.c
 * @brief HDC302x Temperature & Humidity Sensor Driver Implementation
 *
 * Based on TI HDC302x datasheet, Section 8.5.1–8.5.5.
 */

#include "humidity_temp_sensor.h"
#include <string.h>

/* ==== CRC8 CALCULATION (Polynomial 0x31, Init 0xFF) ==== */
static uint8_t HDC302x_CRC8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t j = 0; j < len; j++) {
        crc ^= data[j];
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ==== INTERNAL HELPER: Write 16-bit Command ==== */
static HAL_StatusTypeDef HDC302x_WriteCmd(HDC302x_HandleTypeDef *dev, uint16_t cmd)
{
    uint8_t tx[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return HAL_I2C_Master_Transmit(dev->hi2c, dev->i2c_addr, tx, 2, 100);
}

/* ==== DEVICE INITIALIZATION ==== */
HAL_StatusTypeDef HDC302x_Init(HDC302x_HandleTypeDef *dev,I2C_HandleTypeDef *hi2c, uint8_t address)
{
    dev->hi2c=hi2c;
    dev->i2c_addr = address;
    
    // Soft reset (optional but recommended)
    if (HDC302x_WriteCmd(dev, HDC302x_CMD_SOFT_RESET) != HAL_OK)
        return HAL_ERROR;
    HAL_Delay(3); // tRESET ~ 2.2ms (datasheet)
    return HAL_OK;
}

/* ==== SINGLE MEASUREMENT READ ==== */
HAL_StatusTypeDef HDC302x_ReadData(HDC302x_HandleTypeDef *dev, float *temperature, float *humidity)
{
    uint8_t rx[6];
    HAL_StatusTypeDef ret;

    // Step 1: Trigger measurement (LPM0)
    if (HDC302x_WriteCmd(dev, HDC302x_CMD_TRIGGER_LPM0) != HAL_OK)
        return HAL_ERROR;

    // Step 2: Wait t_MEAS (typ. 3.7ms–14.2ms depending on LPM)
    HAL_Delay(15);

    // Step 3: Read 6 bytes (T_MSB, T_LSB, CRC_T, RH_MSB, RH_LSB, CRC_RH)
    ret = HAL_I2C_Master_Receive(dev->hi2c, dev->i2c_addr, rx, 6, 100);
    if (ret != HAL_OK)
        return HAL_ERROR;

    // Step 4: CRC validation
    if (HDC302x_CRC8(&rx[0], 2) != rx[2] || HDC302x_CRC8(&rx[3], 2) != rx[5])
        return HAL_ERROR;

    // Step 5: Raw to physical conversion
    uint16_t rawT = ((uint16_t)rx[0] << 8) | rx[1];
    uint16_t rawRH = ((uint16_t)rx[3] << 8) | rx[4];

    *temperature = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    *humidity = 100.0f * ((float)rawRH / 65535.0f);

    // Defensive filter (if device not ready, rawT=0x0000, rawRH=0xFFFF)
    if (rawT == 0x0000 || rawRH == 0xFFFF)
        return HAL_ERROR;

    return HAL_OK;
}
