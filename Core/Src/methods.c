#include "methods.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "i2c.h"


/**
 * @brief 诊断 I2C 错误并尝试自动恢复
 * @param hi2c I2C句柄
 * @param sensor_name 传感器名称（用于打印识别）
 * @param status HAL操作返回值
 */

void RGB_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOC clock
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // Configure PC0, PC1, PC2 as output pins
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Turn on all LEDs initially
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_RESET);
}
void RGB_LED_Blink(void)
{
    // Red
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_Delay(10);

    // Green
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_Delay(10);

    // Blue
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_Delay(10);
}
void USART_Print(const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    // 发送格式化的字符串通过UART
    HAL_UART_Transmit(&huart1, (uint8_t *)buffer, strlen(buffer), 100);
}
void Send_Raw_Bytes(uint16_t data)
{
    uint8_t bytes[2];
    
    // 将16位数据拆分为2个字节
    bytes[0] = (uint8_t)(data & 0xFF);        // 低字节 (LSB)
    bytes[1] = (uint8_t)((data >> 8) & 0xFF); // 高字节 (MSB)
    
    // 发送字节数据
    HAL_UART_Transmit(&huart1, bytes, 2, HAL_MAX_DELAY);
}

void Send_Buffer_Bytes(uint16_t *buffer, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
    {
        Send_Raw_Bytes(buffer[i]);
    }
}

void I2C_Scan(void)
{
    USB_Print("Starting I2C scan...\r\n");

    for (uint8_t addr = 0x03; addr <= 0x77; addr++)
    {
        // Check if a device responds at this address
        if (HAL_I2C_IsDeviceReady(&hi2c1, (addr << 1), 1, HAL_MAX_DELAY) == HAL_OK)
        {
            USB_Print("I2C device found at address: 0x%02X\r\n", addr);
        }
    }

    USB_Print("I2C scan complete.\r\n");
}

void USB_Print(const char *format, ...)
{
    static char buffer[256];  // static buffer（USB需要持久指针）
    
    // ✅ 快速检查：如果USB正在发送，直接返回（避免覆盖buffer）
    extern USBD_HandleTypeDef hUsbDeviceFS;
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    if (hcdc && hcdc->TxState != 0) {
        return;  // Busy，丢弃本次数据（不阻塞）
    }
    
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0 && len < sizeof(buffer)) {
        CDC_Transmit_FS((uint8_t*)buffer, len);  // 直接发送
    }
}
