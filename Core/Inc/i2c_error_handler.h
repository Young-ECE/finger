/**
  ******************************************************************************
  * @file    i2c_error_handler.h
  * @brief   Header file for I2C error detection and recovery
  ******************************************************************************
  */

#ifndef __I2C_ERROR_HANDLER_H
#define __I2C_ERROR_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* Error statistics variables */
extern volatile uint32_t i2c_error_count;
extern volatile uint32_t i2c_timeout_count;
extern volatile uint32_t i2c_bus_error_count;
extern volatile uint32_t i2c_arbitration_lost_count;
extern volatile uint32_t i2c_recovery_count;

/* Function prototypes */
HAL_StatusTypeDef I2C_RecoverBus(void);
void I2C_GetErrorStats(uint32_t *total_errors, uint32_t *timeouts,
                       uint32_t *bus_errors, uint32_t *recoveries);
void I2C_ResetErrorStats(void);

/* New health monitoring functions */
HAL_StatusTypeDef I2C_HealthCheck(void);
HAL_StatusTypeDef I2C_ForceReset(void);
void I2C_ClearBusyFlag(void);
uint8_t I2C_IsHealthy(void);

#ifdef __cplusplus
}
#endif

#endif /* __I2C_ERROR_HANDLER_H */
