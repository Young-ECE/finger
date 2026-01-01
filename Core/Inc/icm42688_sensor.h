/**
 * @file icm42688_sensor.h
 * @brief TDK InvenSense ICM42688 6-Axis IMU Driver
 * @version 1.0
 * @date 2025-11
 *
 * Reference: ICM-42688-P Datasheet (DS-000347)
 * Interface: I2C, supports 0x68/0x69
 */

#ifndef __ICM42688_SENSOR_H__
#define __ICM42688_SENSOR_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==== ICM42688 I2C ADDRESSES ==== */
#define ICM42688_ADDR_68   (0x68 << 1)
#define ICM42688_ADDR_69   (0x69 << 1)

/* ==== BANK 0 REGISTER MAP ==== */
#define ICM42688_REG_DEVICE_CONFIG      0x11
#define ICM42688_REG_INT_CONFIG         0x14
#define ICM42688_REG_FIFO_CONFIG        0x16
#define ICM42688_REG_TEMP_DATA1         0x1D
#define ICM42688_REG_TEMP_DATA0         0x1E
#define ICM42688_REG_ACCEL_DATA_X1      0x1F
#define ICM42688_REG_ACCEL_DATA_X0      0x20
#define ICM42688_REG_ACCEL_DATA_Y1      0x21
#define ICM42688_REG_ACCEL_DATA_Y0      0x22
#define ICM42688_REG_ACCEL_DATA_Z1      0x23
#define ICM42688_REG_ACCEL_DATA_Z0      0x24
#define ICM42688_REG_GYRO_DATA_X1       0x25
#define ICM42688_REG_GYRO_DATA_X0       0x26
#define ICM42688_REG_GYRO_DATA_Y1       0x27
#define ICM42688_REG_GYRO_DATA_Y0       0x28
#define ICM42688_REG_GYRO_DATA_Z1       0x29
#define ICM42688_REG_GYRO_DATA_Z0       0x2A
#define ICM42688_REG_INT_STATUS         0x2D
#define ICM42688_REG_PWR_MGMT0          0x4E
#define ICM42688_REG_GYRO_CONFIG0       0x4F
#define ICM42688_REG_ACCEL_CONFIG0      0x50
#define ICM42688_REG_GYRO_CONFIG1       0x51
#define ICM42688_REG_ACCEL_CONFIG1      0x53
#define ICM42688_REG_INT_CONFIG0        0x63
#define ICM42688_REG_INT_CONFIG1        0x64
#define ICM42688_REG_INT_SOURCE0        0x65
#define ICM42688_REG_WHO_AM_I           0x75
#define ICM42688_REG_BANK_SEL           0x76

/* ==== WHO_AM_I VALUE ==== */
#define ICM42688_WHO_AM_I_VALUE         0x47

/* ==== POWER MANAGEMENT ==== */
#define ICM42688_PWR_GYRO_MODE_OFF      (0x00 << 2)
#define ICM42688_PWR_GYRO_MODE_STANDBY  (0x01 << 2)
#define ICM42688_PWR_GYRO_MODE_LN       (0x03 << 2)  // Low Noise
#define ICM42688_PWR_ACCEL_MODE_OFF     (0x00 << 0)
#define ICM42688_PWR_ACCEL_MODE_LP      (0x02 << 0)  // Low Power
#define ICM42688_PWR_ACCEL_MODE_LN      (0x03 << 0)  // Low Noise

/* ==== GYRO FULL SCALE RANGE ==== */
#define ICM42688_GYRO_FS_2000DPS        (0x00 << 5)
#define ICM42688_GYRO_FS_1000DPS        (0x01 << 5)
#define ICM42688_GYRO_FS_500DPS         (0x02 << 5)
#define ICM42688_GYRO_FS_250DPS         (0x03 << 5)

/* ==== ACCEL FULL SCALE RANGE ==== */
#define ICM42688_ACCEL_FS_16G           (0x00 << 5)
#define ICM42688_ACCEL_FS_8G            (0x01 << 5)
#define ICM42688_ACCEL_FS_4G            (0x02 << 5)
#define ICM42688_ACCEL_FS_2G            (0x03 << 5)

/* ==== ODR (Output Data Rate) ==== */
#define ICM42688_ODR_32KHZ              0x01
#define ICM42688_ODR_16KHZ              0x02
#define ICM42688_ODR_8KHZ               0x03
#define ICM42688_ODR_4KHZ               0x04
#define ICM42688_ODR_2KHZ               0x05
#define ICM42688_ODR_1KHZ               0x06
#define ICM42688_ODR_200HZ              0x07
#define ICM42688_ODR_100HZ              0x08
#define ICM42688_ODR_50HZ               0x09
#define ICM42688_ODR_25HZ               0x0A
#define ICM42688_ODR_12_5HZ             0x0B

/* ==== STRUCTURE ==== */


typedef struct {
    float x;
    float y;
    float z;
} ICM42688_ScaledData;
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t i2c_addr;
    float gyro_scale;   // Scale factor for gyro (DPS/LSB)
    float accel_scale;  // Scale factor for accel (g/LSB)
	
	// === ??:DMA ???? ===
    uint8_t dma_buffer[14];
	
	// === ??:??????????? ===
    ICM42688_ScaledData accel_raw;
    ICM42688_ScaledData gyro_raw;
    float temp_raw;
	
} ICM42688_HandleTypeDef;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} ICM42688_RawData;



/* ==== FUNCTION DECLARATIONS ==== */
HAL_StatusTypeDef ICM42688_Init(ICM42688_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, uint8_t address);
HAL_StatusTypeDef ICM42688_ReadAccel(ICM42688_HandleTypeDef *dev, ICM42688_ScaledData *accel);
HAL_StatusTypeDef ICM42688_ReadGyro(ICM42688_HandleTypeDef *dev, ICM42688_ScaledData *gyro);
HAL_StatusTypeDef ICM42688_ReadTemp(ICM42688_HandleTypeDef *dev, float *temperature);
HAL_StatusTypeDef ICM42688_ReadAll(ICM42688_HandleTypeDef *dev, ICM42688_ScaledData *accel,
                                    ICM42688_ScaledData *gyro, float *temperature);

// ?? DMA ??????
HAL_StatusTypeDef ICM42688_Start_DMA_Read(ICM42688_HandleTypeDef *dev);
// ?? DMA ????????
void ICM42688_DMA_Callback(ICM42688_HandleTypeDef *dev);

#ifdef __cplusplus
}
#endif

#endif /* __ICM42688_SENSOR_H__ */
