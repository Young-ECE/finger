from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]

main_c = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8", errors="ignore")
main_h = (ROOT / "Core/Inc/main.h").read_text(encoding="utf-8", errors="ignore")
ioc = (ROOT / "finger.ioc").read_text(encoding="utf-8", errors="ignore")
tim_h = (ROOT / "Core/Inc/tim.h").read_text(encoding="utf-8", errors="ignore")
tim_c = (ROOT / "Core/Src/tim.c").read_text(encoding="utf-8", errors="ignore")
hal_conf = (ROOT / "Core/Inc/stm32f4xx_hal_conf.h").read_text(encoding="utf-8", errors="ignore")
my_application_h = (ROOT / "Core/Inc/my_application.h").read_text(encoding="utf-8", errors="ignore")
my_application_c = (ROOT / "Core/Src/my_application.c").read_text(encoding="utf-8", errors="ignore")
cdc_if = (ROOT / "USB_DEVICE/App/usbd_cdc_if.c").read_text(encoding="utf-8", errors="ignore")

tim_dma_usage = re.compile(r"\bHAL_TIM_\w*DMA\b")
tim2_dma_usage = re.compile(r"\bHAL_TIM_\w*DMA\b[^\n]*&htim2\b|&htim2[^\n]*\bHAL_TIM_\w*DMA\b")
tim2_dma_ioc = re.compile(r"(?im)^TIM2\..*DMA|^Dma\..*TIM2")

assert "MX_TIM2_Init();" in main_c, "main.c does not initialize TIM2"
assert '#include "tim.h"' in main_c, "main.c does not include tim.h"
assert (
    "HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);" in main_c
    or "HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);" in tim_c
), "PWM channel 1 is not started"
assert "TIM2" in ioc, "finger.ioc does not include TIM2"
assert "LCD1_Pin" in main_h and "LED2_Pin" in main_h, "main.h is missing PWM pin defines"
assert "extern TIM_HandleTypeDef htim2;" in tim_h, "tim.h does not export htim2"
assert "HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4)" in tim_c, "tim.c does not configure all four PWM channels"
assert "\n#define HAL_TIM_MODULE_ENABLED\n" in ("\n" + hal_conf + "\n"), "stm32f4xx_hal_conf.h does not enable HAL_TIM_MODULE_ENABLED"
assert not tim2_dma_ioc.search(ioc), "finger.ioc appears to enable TIM2 DMA"
assert not tim2_dma_usage.search(main_c), "main.c contains TIM2 DMA usage"
assert not tim2_dma_usage.search(tim_c), "tim.c contains TIM2 DMA usage"
assert not tim2_dma_usage.search(my_application_c), "my_application.c contains TIM2 DMA usage"
assert "void My_Application_OnUsbReceived(uint8_t *data, uint32_t len);" in my_application_h, "application USB receive hook missing"
assert "PwmControl_FeedBytes(" in cdc_if or "PwmControl_FeedBytes(" in my_application_c, "PWM control parser is not wired into USB receive handling"
assert "PwmControl_FeedBytes(" in my_application_c, "my_application.c does not parse PWM control bytes"
assert "__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4" in my_application_c, "my_application.c does not apply PWM duty"
assert "My_Application_OnUsbReceived(Buf, *Len);" in cdc_if, "CDC receive path does not forward data to the application"

print("pwm wiring checks passed")
