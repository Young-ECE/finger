/**
 * @file humidity_temp_sensor.c
 * @brief BME280 Temperature, Humidity & Pressure Sensor Driver Implementation
 *
 * Based on Bosch BME280 datasheet (BST-BME280-DS002)
 */

#include "humidity_temp_sensor.h"
#include <string.h>

/* ==== INTERNAL HELPER: Write Register ==== */
static HAL_StatusTypeDef BME280_WriteReg(BME280_HandleTypeDef *dev, uint8_t reg, uint8_t value)
{
    HAL_StatusTypeDef status;
    status = TCA9548A_SelectChannel(dev->hi2c, dev->mux_addr, dev->mux_channel);
    if (status != HAL_OK) return status;
    return HAL_I2C_Mem_Write(dev->hi2c, dev->sensor_addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
}

/* ==== INTERNAL HELPER: Read Register ==== */
static HAL_StatusTypeDef BME280_ReadReg(BME280_HandleTypeDef *dev, uint8_t reg, uint8_t *buffer, uint8_t len)
{
    HAL_StatusTypeDef status;
    status = TCA9548A_SelectChannel(dev->hi2c, dev->mux_addr, dev->mux_channel);
    if (status != HAL_OK) return status;
    return HAL_I2C_Mem_Read(dev->hi2c, dev->sensor_addr, reg, I2C_MEMADD_SIZE_8BIT, buffer, len, 100);
}

/* ==== TCA9548A: SELECT MULTIPLEXER CHANNEL ==== */
HAL_StatusTypeDef TCA9548A_SelectChannel(I2C_HandleTypeDef *hi2c, uint8_t mux_addr, uint8_t channel)
{
    uint8_t control_byte;

    if (channel > 7)
        return HAL_ERROR;

    control_byte = (1 << channel);

    /* Optimized: single write, no verification, no retry
     * This reduces switching time from ~1-5ms to ~0.5ms
     * to avoid blocking I2S interrupts (8kHz, 125μs period)
     */
    return HAL_I2C_Master_Transmit(hi2c, mux_addr, &control_byte, 1, 10);
}

/* ==== BME280: READ CALIBRATION DATA ==== */
static HAL_StatusTypeDef BME280_ReadCalibration(BME280_HandleTypeDef *dev)
{
    uint8_t calib[32];

    // Read temperature and pressure calibration (0x88-0x9F)
    if (BME280_ReadReg(dev, BME280_REG_CALIB00, calib, 26) != HAL_OK)
        return HAL_ERROR;

    dev->calib.dig_T1 = (calib[1] << 8) | calib[0];
    dev->calib.dig_T2 = (calib[3] << 8) | calib[2];
    dev->calib.dig_T3 = (calib[5] << 8) | calib[4];
    dev->calib.dig_P1 = (calib[7] << 8) | calib[6];
    dev->calib.dig_P2 = (calib[9] << 8) | calib[8];
    dev->calib.dig_P3 = (calib[11] << 8) | calib[10];
    dev->calib.dig_P4 = (calib[13] << 8) | calib[12];
    dev->calib.dig_P5 = (calib[15] << 8) | calib[14];
    dev->calib.dig_P6 = (calib[17] << 8) | calib[16];
    dev->calib.dig_P7 = (calib[19] << 8) | calib[18];
    dev->calib.dig_P8 = (calib[21] << 8) | calib[20];
    dev->calib.dig_P9 = (calib[23] << 8) | calib[22];
    dev->calib.dig_H1 = calib[25];

    // Read humidity calibration (0xE1-0xE7)
    if (BME280_ReadReg(dev, BME280_REG_CALIB26, calib, 7) != HAL_OK)
        return HAL_ERROR;

    dev->calib.dig_H2 = (calib[1] << 8) | calib[0];
    dev->calib.dig_H3 = calib[2];
    dev->calib.dig_H4 = (calib[3] << 4) | (calib[4] & 0x0F);
    dev->calib.dig_H5 = (calib[5] << 4) | (calib[4] >> 4);
    dev->calib.dig_H6 = calib[6];

    return HAL_OK;
}

/* ==== BME280: COMPENSATE TEMPERATURE ==== */
static int32_t BME280_CompensateTemperature(BME280_HandleTypeDef *dev, int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)dev->calib.dig_T1 << 1))) * ((int32_t)dev->calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dev->calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)dev->calib.dig_T1))) >> 12) * ((int32_t)dev->calib.dig_T3)) >> 14;
    dev->calib.t_fine = var1 + var2;
    T = (dev->calib.t_fine * 5 + 128) >> 8;
    return T;
}

/* ==== BME280: COMPENSATE PRESSURE ==== */
static uint32_t BME280_CompensatePressure(BME280_HandleTypeDef *dev, int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)dev->calib.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dev->calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)dev->calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)dev->calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dev->calib.dig_P3) >> 8) + ((var1 * (int64_t)dev->calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dev->calib.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dev->calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dev->calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dev->calib.dig_P7) << 4);
    return (uint32_t)p;
}

/* ==== BME280: COMPENSATE HUMIDITY ==== */
static uint32_t BME280_CompensateHumidity(BME280_HandleTypeDef *dev, int32_t adc_H)
{
    int32_t v_x1_u32r;
    v_x1_u32r = (dev->calib.t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dev->calib.dig_H4) << 20) - (((int32_t)dev->calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dev->calib.dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dev->calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)dev->calib.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dev->calib.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    return (uint32_t)(v_x1_u32r >> 12);
}

/* ==== DEVICE INITIALIZATION ==== */
HAL_StatusTypeDef BME280_Init(BME280_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c,
                               uint8_t mux_addr, uint8_t mux_channel, uint8_t sensor_addr)
{
    uint8_t chip_id;
    HAL_StatusTypeDef status;

    dev->hi2c = hi2c;
    dev->mux_addr = mux_addr;
    dev->mux_channel = mux_channel;
    dev->sensor_addr = sensor_addr;

    // Select multiplexer channel
    status = TCA9548A_SelectChannel(hi2c, mux_addr, mux_channel);
    if (status != HAL_OK) {
        return HAL_ERROR;  // Mux selection failed
    }
    HAL_Delay(1);

    // Verify chip ID
    status = BME280_ReadReg(dev, BME280_REG_ID, &chip_id, 1);
    if (status != HAL_OK) {
        return HAL_ERROR;  // I2C read failed
    }

    if (chip_id != BME280_CHIP_ID) {
        return HAL_ERROR;  // Wrong chip ID (expected 0x60)
    }

    // Soft reset
    if (BME280_WriteReg(dev, BME280_REG_RESET, BME280_CMD_SOFT_RESET) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(10);

    // Read calibration data
    if (BME280_ReadCalibration(dev) != HAL_OK)
        return HAL_ERROR;

    // Configure humidity oversampling (must be written before CTRL_MEAS)
    if (BME280_WriteReg(dev, BME280_REG_CTRL_HUM, BME280_OVERSAMPLE_1X) != HAL_OK)
        return HAL_ERROR;

    // Configure measurement: temp x1, pressure x1, forced mode
    uint8_t ctrl_meas = (BME280_OVERSAMPLE_1X << 5) | (BME280_OVERSAMPLE_1X << 2) | BME280_MODE_FORCED;
    if (BME280_WriteReg(dev, BME280_REG_CTRL_MEAS, ctrl_meas) != HAL_OK)
        return HAL_ERROR;

    // Configure: standby 0.5ms, filter off
    if (BME280_WriteReg(dev, BME280_REG_CONFIG, 0x00) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

/* ==== SINGLE MEASUREMENT READ ==== */
HAL_StatusTypeDef BME280_ReadData(BME280_HandleTypeDef *dev, float *temperature,
                                   float *humidity, float *pressure)
{
    uint8_t data[8];
    int32_t adc_T, adc_P, adc_H;
    int32_t temp_comp;
    uint32_t press_comp, hum_comp;
    HAL_StatusTypeDef status;

    // Force re-select channel before every read (don't trust state)
    status = TCA9548A_SelectChannel(dev->hi2c, dev->mux_addr, dev->mux_channel);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    // Trigger forced mode measurement - single attempt, no retry
    uint8_t ctrl_meas = (BME280_OVERSAMPLE_1X << 5) | (BME280_OVERSAMPLE_1X << 2) | BME280_MODE_FORCED;
    status = BME280_WriteReg(dev, BME280_REG_CTRL_MEAS, ctrl_meas);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    // Wait for measurement to complete (reduced to 15ms for faster response)
    HAL_Delay(15);

    // Read all sensor data (0xF7-0xFE) - single attempt, no retry
    status = BME280_ReadReg(dev, BME280_REG_PRESS_MSB, data, 8);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    // Parse raw ADC values
    adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | ((int32_t)data[2] >> 4);
    adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | ((int32_t)data[5] >> 4);
    adc_H = ((int32_t)data[6] << 8) | (int32_t)data[7];

    // Sanity check: ADC values should not be all zeros (indicates read error)
    if (adc_T == 0 && adc_P == 0 && adc_H == 0) {
        return HAL_ERROR;
    }

    // Compensate temperature (this also updates t_fine)
    temp_comp = BME280_CompensateTemperature(dev, adc_T);
    *temperature = temp_comp / 100.0f;

    // Compensate pressure (requires t_fine from temperature)
    press_comp = BME280_CompensatePressure(dev, adc_P);
    *pressure = press_comp / 25600.0f; // Pa / 256 -> hPa

    // Compensate humidity (requires t_fine from temperature)
    hum_comp = BME280_CompensateHumidity(dev, adc_H);
    *humidity = hum_comp / 1024.0f;

    return HAL_OK;
}
