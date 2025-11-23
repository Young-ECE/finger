/**
 * @file humidity_temp_sensor.h
 * @brief Bosch BME280 Temperature, Humidity & Pressure Sensor Driver with TCA9548A Multiplexer
 * @version 2.0
 * @date 2025-11
 *
 * Reference: Bosch BME280 Datasheet (BST-BME280-DS002)
 * Reference: TI TCA9548A Datasheet (I2C Multiplexer)
 * Interface: I2C, 8x BME280 via TCA9548A multiplexer
 */

#ifndef __HUMIDITY_TEMP_SENSOR_H__
#define __HUMIDITY_TEMP_SENSOR_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==== TCA9548A I2C MULTIPLEXER ADDRESSES ==== */
#define TCA9548A_ADDR_70   (0x70 << 1)
#define TCA9548A_ADDR_71   (0x71 << 1)
#define TCA9548A_ADDR_72   (0x72 << 1)
#define TCA9548A_ADDR_73   (0x73 << 1)
#define TCA9548A_ADDR_74   (0x74 << 1)
#define TCA9548A_ADDR_75   (0x75 << 1)
#define TCA9548A_ADDR_76   (0x76 << 1)
#define TCA9548A_ADDR_77   (0x77 << 1)

/* ==== BME280 I2C ADDRESSES ==== */
#define BME280_ADDR_76     (0x76 << 1)
#define BME280_ADDR_77     (0x77 << 1)

/* ==== BME280 REGISTER MAP ==== */
#define BME280_REG_ID              0xD0
#define BME280_REG_RESET           0xE0
#define BME280_REG_CTRL_HUM        0xF2
#define BME280_REG_STATUS          0xF3
#define BME280_REG_CTRL_MEAS       0xF4
#define BME280_REG_CONFIG          0xF5
#define BME280_REG_PRESS_MSB       0xF7
#define BME280_REG_TEMP_MSB        0xFA
#define BME280_REG_HUM_MSB         0xFD

/* ==== BME280 CALIBRATION REGISTERS ==== */
#define BME280_REG_CALIB00         0x88  // Start of T1-H1 calibration
#define BME280_REG_CALIB26         0xE1  // Start of H2-H6 calibration

/* ==== BME280 CHIP ID ==== */
#define BME280_CHIP_ID             0x60

/* ==== BME280 COMMANDS ==== */
#define BME280_CMD_SOFT_RESET      0xB6

/* ==== BME280 MODES ==== */
#define BME280_MODE_SLEEP          0x00
#define BME280_MODE_FORCED         0x01
#define BME280_MODE_NORMAL         0x03

/* ==== OVERSAMPLING SETTINGS ==== */
#define BME280_OVERSAMPLE_1X       0x01
#define BME280_OVERSAMPLE_2X       0x02
#define BME280_OVERSAMPLE_4X       0x03
#define BME280_OVERSAMPLE_8X       0x04
#define BME280_OVERSAMPLE_16X      0x05

/* ==== STRUCTURE ==== */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
    int32_t  t_fine;  // Used for temperature compensation
} BME280_CalibData;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t mux_addr;       // TCA9548A multiplexer address
    uint8_t mux_channel;    // Channel number (0-7)
    uint8_t sensor_addr;    // BME280 sensor address
    BME280_CalibData calib; // Calibration data
} BME280_HandleTypeDef;

/* ==== FUNCTION DECLARATIONS ==== */
HAL_StatusTypeDef TCA9548A_SelectChannel(I2C_HandleTypeDef *hi2c, uint8_t mux_addr, uint8_t channel);
HAL_StatusTypeDef BME280_Init(BME280_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c,
                               uint8_t mux_addr, uint8_t mux_channel, uint8_t sensor_addr);
HAL_StatusTypeDef BME280_ReadData(BME280_HandleTypeDef *dev, float *temperature,
                                   float *humidity, float *pressure);

#ifdef __cplusplus
}
#endif

#endif /* __HUMIDITY_TEMP_SENSOR_H__ */
