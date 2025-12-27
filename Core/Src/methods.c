#include "methods.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "i2c.h"



void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 诊断 I2C 错误并尝试自动恢复
 * @param hi2c I2C句柄
 * @param sensor_name 传感器名称（用于打印识别）
 * @param status HAL操作返回值
 */
void I2C_Diagnose_And_Recover(I2C_HandleTypeDef *hi2c, char* sensor_name, HAL_StatusTypeDef status)
{
    if (status != HAL_OK)
    {
			
        static char error_buf[256]; // 使用 static 减小栈压力			
        // 1. 抓取快照
        uint32_t err = HAL_I2C_GetError(hi2c);
        uint32_t sr1 = hi2c->Instance->SR1;
        uint32_t sr2 = hi2c->Instance->SR2;

        // 2. 打印详细报告 (VOFA+ 文本窗口可见)
        int len = sprintf(error_buf, 
            "\r\n!!! I2C Error @ %s !!!\r\n"
            "Status: %d | ErrorCode: 0x%02X | SR1: 0x%04X | SR2: 0x%04X\r\n"
            "Attempting Bus Recovery...\r\n",
            sensor_name, (int)status, (unsigned int)err, (unsigned int)sr1, (unsigned int)sr2);
				
//				CDC_Transmit_FS((uint8_t*)error_buf, len);

//        // 3. 具体的错误解读
//        if (err & HAL_I2C_ERROR_AF)   USB_Print("-> Reason: Acknowledge Failure (NACK)\r\n");
//        if (err & HAL_I2C_ERROR_BERR) USB_Print("-> Reason: Bus Error (Interference?)\r\n");
//        if (err & HAL_I2C_ERROR_ARLO) USB_Print("-> Reason: Arbitration Lost\r\n");
//        if (sr2 & I2C_SR2_BUSY)       USB_Print("-> Line State: BUSY (SDA stuck low?)\r\n");

        // 4. 尝试物理恢复 (使用你已有的 BusRecover)
        
        I2C_BusRecover(); 
        HAL_I2C_DeInit(hi2c);
        MX_I2C1_Init();
        
    }
}


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

    // // White (all on)
    // HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_SET);
    // HAL_Delay(200);

    // // Off
    // HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_RESET);
    // HAL_Delay(200);
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
