/**
  ******************************************************************************
  * @file    i2c_error_handler.c
  * @brief   I2C error detection and recovery implementation
  ******************************************************************************
  * @attention
  *
  * This file implements I2C bus error recovery mechanisms to prevent
  * permanent bus lockup when communication errors occur.
  *
  ******************************************************************************
  */

#include "main.h"
#include "i2c.h"
#include "i2c_dma_manager.h"

/* Error statistics counters */
volatile uint32_t i2c_error_count = 0;
volatile uint32_t i2c_timeout_count = 0;
volatile uint32_t i2c_bus_error_count = 0;
volatile uint32_t i2c_arbitration_lost_count = 0;
volatile uint32_t i2c_recovery_count = 0;

/* Health monitoring */
static volatile uint32_t i2c_last_successful_transaction = 0;
static volatile uint32_t i2c_consecutive_failures = 0;
#define MAX_CONSECUTIVE_FAILURES 50  // Increased threshold - be more conservative
#define HEALTH_CHECK_INTERVAL_MS 5000  // Check every 5 seconds

/**
  * @brief  I2C error callback - automatically called by HAL on I2C errors
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure
  * @retval None
  */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;

    i2c_error_count++;

    uint32_t error = HAL_I2C_GetError(hi2c);

    /* Log specific error types for diagnostics */
    if (error & HAL_I2C_ERROR_TIMEOUT) {
        i2c_timeout_count++;
    }
    if (error & HAL_I2C_ERROR_BERR) {
        i2c_bus_error_count++;
    }
    if (error & HAL_I2C_ERROR_ARLO) {
        i2c_arbitration_lost_count++;
    }

    /* Reset I2C peripheral on critical errors */
    if (error & (HAL_I2C_ERROR_BERR | HAL_I2C_ERROR_ARLO | HAL_I2C_ERROR_TIMEOUT)) {
        i2c_recovery_count++;

        /* Disable I2C peripheral */
        __HAL_I2C_DISABLE(hi2c);

        /* Force reset of I2C peripheral */
        __HAL_RCC_I2C1_FORCE_RESET();
        HAL_Delay(2);
        __HAL_RCC_I2C1_RELEASE_RESET();

        /* Re-initialize I2C */
        MX_I2C1_Init();
    }
    
    /* 通知DMA管理器状态机停止 */
    I2C_DMA_HandleError();
}

/**
  * @brief  Recovers I2C bus by generating clock pulses to clear stuck slave
  * @note   This function should be called when I2C bus is stuck (SDA/SCL low)
  * @retval HAL_OK if recovery successful, HAL_ERROR otherwise
  */
HAL_StatusTypeDef I2C_RecoverBus(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* De-initialize I2C to release pins */
    HAL_I2C_DeInit(&hi2c1);

    /* Reconfigure I2C pins as GPIO open-drain output */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_9;  // PB6=SCL, PB9=SDA
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Ensure both lines are high initially */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);  // SCL high
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);  // SDA high
    HAL_Delay(2);

    /* Generate 9 clock pulses on SCL to clear any stuck slave device */
    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);  // SCL low
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);    // SCL high
        HAL_Delay(1);
    }

    /* Generate I2C STOP condition */
    /* SDA goes from low to high while SCL is high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);  // SDA low
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);    // SCL high
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);    // SDA high (STOP)
    HAL_Delay(2);

    /* Re-initialize I2C peripheral */
    MX_I2C1_Init();
    return HAL_OK;
}

/**
  * @brief  Gets I2C error statistics
  * @param  total_errors Total error count
  * @param  timeouts Timeout error count
  * @param  bus_errors Bus error count
  * @param  recoveries Recovery attempt count
  * @retval None
  */
void I2C_GetErrorStats(uint32_t *total_errors, uint32_t *timeouts,
                       uint32_t *bus_errors, uint32_t *recoveries)
{
    if (total_errors) *total_errors = i2c_error_count;
    if (timeouts) *timeouts = i2c_timeout_count;
    if (bus_errors) *bus_errors = i2c_bus_error_count;
    if (recoveries) *recoveries = i2c_recovery_count;
}

/**
  * @brief  Resets I2C error statistics
  * @retval None
  */
void I2C_ResetErrorStats(void)
{
    i2c_error_count = 0;
    i2c_timeout_count = 0;
    i2c_bus_error_count = 0;
    i2c_arbitration_lost_count = 0;
    i2c_recovery_count = 0;
    i2c_consecutive_failures = 0;
}

/**
  * @brief  Clears the BUSY flag if stuck (known STM32 I2C HAL bug workaround)
  * @retval None
  */
void I2C_ClearBusyFlag(void)
{
    /* Force clear BUSY flag by toggling PE bit */
    if (hi2c1.Instance->SR2 & I2C_SR2_BUSY) {
        /* Disable peripheral */
        hi2c1.Instance->CR1 &= ~I2C_CR1_PE;
        
        /* Small delay */
        for (volatile int i = 0; i < 100; i++);
        
        /* Re-enable peripheral */
        hi2c1.Instance->CR1 |= I2C_CR1_PE;
    }
}

/**
  * @brief  Forces a complete I2C reset (more aggressive than error callback)
  * @retval HAL_OK if successful
  */
HAL_StatusTypeDef I2C_ForceReset(void)
{
    i2c_recovery_count++;
    
    /* Disable I2C peripheral */
    __HAL_I2C_DISABLE(&hi2c1);
    
    /* Force GPIO reset to clear any bus lock */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_9;  // SCL, SDA
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* Generate 16 clock pulses (more than standard 9) */
    for (int i = 0; i < 16; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        for (volatile int d = 0; d < 100; d++);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        for (volatile int d = 0; d < 100; d++);
    }
    
    /* Generate multiple STOP conditions to clear any partial transaction */
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);  // SDA low
        for (volatile int d = 0; d < 50; d++);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);    // SCL high
        for (volatile int d = 0; d < 50; d++);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);    // SDA high (STOP)
        for (volatile int d = 0; d < 100; d++);
    }
    
    /* Force reset I2C peripheral registers */
    __HAL_RCC_I2C1_FORCE_RESET();
    HAL_Delay(5);
    __HAL_RCC_I2C1_RELEASE_RESET();
    HAL_Delay(5);
    
    /* Re-initialize I2C */
    MX_I2C1_Init();
    
    /* Clear state */
    hi2c1.State = HAL_I2C_STATE_READY;
    hi2c1.ErrorCode = HAL_I2C_ERROR_NONE;
    
    /* Clear busy flag if still stuck */
    I2C_ClearBusyFlag();
    
    i2c_consecutive_failures = 0;
    i2c_last_successful_transaction = HAL_GetTick();
    
    return HAL_OK;
}

/**
  * @brief  Performs I2C health check by reading TCA9548A multiplexer
  * @retval HAL_OK if bus is healthy, HAL_ERROR otherwise
  */
HAL_StatusTypeDef I2C_HealthCheck(void)
{
    static uint32_t last_check = 0;
    uint32_t now = HAL_GetTick();
    
    /* Only check periodically */
    if (now - last_check < HEALTH_CHECK_INTERVAL_MS) {
        return HAL_OK;
    }
    last_check = now;
    
    /* Try to read from TCA9548A multiplexer (address 0x70) */
    uint8_t dummy;
    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&hi2c1, 0x70 << 1, &dummy, 1, 50);
    
    if (status == HAL_OK) {
        i2c_consecutive_failures = 0;
        i2c_last_successful_transaction = now;
        return HAL_OK;
    } else {
        i2c_consecutive_failures++;
        /* Just track failures, don't auto-reset - let main loop decide */
        return HAL_ERROR;
    }
}

/**
  * @brief  Checks if I2C bus is currently healthy
  * @retval 1 if healthy, 0 if unhealthy
  */
uint8_t I2C_IsHealthy(void)
{
    /* Check if too many consecutive failures */
    if (i2c_consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
        return 0;
    }
    
    /* Check if BUSY flag is stuck */
    if (hi2c1.Instance->SR2 & I2C_SR2_BUSY) {
        if (hi2c1.State == HAL_I2C_STATE_READY) {
            /* BUSY flag stuck while HAL thinks it's ready - unhealthy */
            return 0;
        }
    }
    
    return 1;
}
