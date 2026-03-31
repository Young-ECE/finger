from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]

main_c = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8", errors="ignore")
main_h = (ROOT / "Core/Inc/main.h").read_text(encoding="utf-8", errors="ignore")
ioc = (ROOT / "finger.ioc").read_text(encoding="utf-8", errors="ignore")
tim_h = (ROOT / "Core/Inc/tim.h").read_text(encoding="utf-8", errors="ignore")
tim_c = (ROOT / "Core/Src/tim.c").read_text(encoding="utf-8", errors="ignore")
hal_conf = (ROOT / "Core/Inc/stm32f4xx_hal_conf.h").read_text(encoding="utf-8", errors="ignore")
my_application_c = (ROOT / "Core/Src/my_application.c").read_text(encoding="utf-8", errors="ignore")
tim2_dma_usage = re.compile(r"HAL_TIM_\w*DMA\s*\(\s*[^;]*?&htim2[^;]*?\)", re.S)

assert "MX_TIM2_Init();" in main_c, "main.c does not initialize TIM2"
assert '#include "tim.h"' in main_c, "main.c does not include tim.h"
for channel in range(1, 5):
    assert f"HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_{channel});" in main_c, f"PWM channel {channel} is not started"
for pin_define in ("LCD1_Pin", "LED1_Pin", "LCD2_Pin", "LED2_Pin"):
    assert pin_define in main_h, f"main.h is missing {pin_define}"
assert "extern TIM_HandleTypeDef htim2;" in tim_h, "tim.h does not export htim2"
for channel in range(1, 5):
    assert (
        f"HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_{channel})" in tim_c
    ), f"tim.c does not configure PWM channel {channel}"
assert "\n#define HAL_TIM_MODULE_ENABLED\n" in ("\n" + hal_conf + "\n"), "stm32f4xx_hal_conf.h does not enable HAL_TIM_MODULE_ENABLED"
assert "TIM2 DMA" not in ioc, "finger.ioc has TIM2 DMA enabled"
for entry in (
    "PA0-WKUP.Signal=S_TIM2_CH1_ETR",
    "PA1.Signal=S_TIM2_CH2",
    "PA2.Signal=S_TIM2_CH3",
    "PA3.Signal=S_TIM2_CH4",
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
assert "GPIO_InitStruct.Pin = LCD1_Pin | LED1_Pin | LCD2_Pin | LED2_Pin;" in tim_c, "tim.c does not configure the combined TIM2 pin mask"
assert "GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;" in tim_c, "tim.c does not configure TIM2 alternate function"
assert not tim2_dma_usage.search(main_c), "main.c contains TIM2 DMA usage"
assert not tim2_dma_usage.search(tim_c), "tim.c contains TIM2 DMA usage"
assert not tim2_dma_usage.search(my_application_c), "my_application.c contains TIM2 DMA usage"

print("pwm wiring checks passed")
