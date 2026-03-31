from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

main_c = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8", errors="ignore")
main_h = (ROOT / "Core/Inc/main.h").read_text(encoding="utf-8", errors="ignore")
ioc = (ROOT / "finger.ioc").read_text(encoding="utf-8", errors="ignore")
tim_h = (ROOT / "Core/Inc/tim.h").read_text(encoding="utf-8", errors="ignore")
tim_c = (ROOT / "Core/Src/tim.c").read_text(encoding="utf-8", errors="ignore")

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

print("pwm wiring checks passed")
