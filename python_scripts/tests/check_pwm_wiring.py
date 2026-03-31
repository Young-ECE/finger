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


def normalize_source(text: str) -> str:
    return re.sub(r"\s+", " ", text.replace("\\\n", " "))


normalized_main_c = normalize_source(main_c)
normalized_tim_c = normalize_source(tim_c)
normalized_my_application_c = normalize_source(my_application_c)
tim2_dma_usage = re.compile(r"\bHAL_TIM_\w*DMA\b.*\b&htim2\b|\b&htim2\b.*\bHAL_TIM_\w*DMA\b")

assert "MX_TIM2_Init();" in main_c, "main.c does not initialize TIM2"
assert '#include "tim.h"' in main_c, "main.c does not include tim.h"
for channel in range(1, 5):
    assert f"HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_{channel});" in main_c, f"PWM channel {channel} is not started"
assert "LCD1_Pin" in main_h and "LED2_Pin" in main_h, "main.h is missing PWM pin defines"
assert "extern TIM_HandleTypeDef htim2;" in tim_h, "tim.h does not export htim2"
for channel in range(1, 5):
    assert (
        f"HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_{channel})" in tim_c
    ), f"tim.c does not configure PWM channel {channel}"
assert "\n#define HAL_TIM_MODULE_ENABLED\n" in ("\n" + hal_conf + "\n"), "stm32f4xx_hal_conf.h does not enable HAL_TIM_MODULE_ENABLED"
for entry in (
    "SH.S_TIM2_CH1_ETR.0=TIM2_CH1,PWM Generation1 CH1",
    "SH.S_TIM2_CH2.0=TIM2_CH2,PWM Generation2 CH2",
    "SH.S_TIM2_CH3.0=TIM2_CH3,PWM Generation3 CH3",
    "SH.S_TIM2_CH4.0=TIM2_CH4,PWM Generation4 CH4",
    "TIM2.Channel-PWM\\ Generation1\\ CH1=TIM_CHANNEL_1",
    "TIM2.Channel-PWM\\ Generation2\\ CH2=TIM_CHANNEL_2",
    "TIM2.Channel-PWM\\ Generation3\\ CH3=TIM_CHANNEL_3",
    "TIM2.Channel-PWM\\ Generation4\\ CH4=TIM_CHANNEL_4",
    "TIM2.IPParameters=Channel-PWM Generation1 CH1,Channel-PWM Generation2 CH2,Channel-PWM Generation3 CH3,Channel-PWM Generation4 CH4,Prescaler,Period",
    "TIM2.Period=99",
    "TIM2.Prescaler=71",
):
    assert entry in ioc, f"finger.ioc is missing {entry}"
assert not tim2_dma_usage.search(normalized_main_c), "main.c contains TIM2 DMA usage"
assert not tim2_dma_usage.search(normalized_tim_c), "tim.c contains TIM2 DMA usage"
assert not tim2_dma_usage.search(normalized_my_application_c), "my_application.c contains TIM2 DMA usage"
assert "void My_Application_OnUsbReceived(uint8_t *data, uint32_t len);" in my_application_h, "application USB receive hook missing"
assert "PwmControl_FeedBytes(" in cdc_if or "PwmControl_FeedBytes(" in my_application_c, "PWM control parser is not wired into USB receive handling"
assert "PwmControl_FeedBytes(" in my_application_c, "my_application.c does not parse PWM control bytes"
assert "__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4" in my_application_c, "my_application.c does not apply PWM duty"
assert "My_Application_OnUsbReceived(Buf, *Len);" in cdc_if, "CDC receive path does not forward data to the application"

print("pwm wiring checks passed")
