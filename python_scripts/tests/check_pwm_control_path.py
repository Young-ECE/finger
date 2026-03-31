from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
MY_APPLICATION_H = ROOT / "Core" / "Inc" / "my_application.h"
MY_APPLICATION_C = ROOT / "Core" / "Src" / "my_application.c"
USBD_CDC_IF_C = ROOT / "USB_DEVICE" / "App" / "usbd_cdc_if.c"


def require(pattern: str, text: str, message: str) -> None:
    if re.search(pattern, text, re.M | re.S) is None:
        raise AssertionError(message)


def extract_function_body(text: str, signature_pattern: str) -> str:
    match = re.search(signature_pattern, text, re.M | re.S)
    if match is None:
        raise AssertionError(f"missing function matching {signature_pattern!r}")

    start = text.find("{", match.end())
    if start == -1:
        raise AssertionError(f"missing function body for {signature_pattern!r}")

    depth = 0
    for index in range(start, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]

    raise AssertionError(f"unterminated function body for {signature_pattern!r}")


def main() -> None:
    my_application_h = MY_APPLICATION_H.read_text(encoding="utf-8", errors="ignore")
    my_application_c = MY_APPLICATION_C.read_text(encoding="utf-8", errors="ignore")
    usbd_cdc_if_c = USBD_CDC_IF_C.read_text(encoding="utf-8", errors="ignore")

    require(
        r"void\s+My_Application_OnUsbReceived\s*\(\s*uint8_t\s*\*\s*data\s*,\s*uint32_t\s+len\s*\)\s*;",
        my_application_h,
        "my_application.h is missing the My_Application_OnUsbReceived declaration",
    )
    require(
        r"void\s+My_Application_OnUsbReceived\s*\(\s*uint8_t\s*\*\s*data\s*,\s*uint32_t\s+len\s*\)\s*\{[^{}]*PwmControl_FeedBytes\s*\(\s*&pwm_state\s*,\s*data\s*,\s*len\s*\)\s*;",
        my_application_c,
        "my_application.c does not define My_Application_OnUsbReceived as a thin PwmControl_FeedBytes wrapper",
    )
    require(
        r"My_Application_OnUsbReceived\s*\(\s*Buf\s*,\s*\*Len\s*\)\s*;",
        usbd_cdc_if_c,
        "usbd_cdc_if.c does not forward CDC_Receive_FS bytes to My_Application_OnUsbReceived",
    )
    require(
        r"static\s+uint8_t\s+PwmControl_ConsumeSnapshot\s*\(\s*PwmControlSnapshot\s*\*\s*snapshot\s*\)",
        my_application_c,
        "my_application.c is missing the dedicated snapshot-consume helper",
    )
    consume_block = extract_function_body(
        my_application_c,
        r"static\s+uint8_t\s+PwmControl_ConsumeSnapshot\s*\(\s*PwmControlSnapshot\s*\*\s*snapshot\s*\)",
    )
    assert "__disable_irq()" in consume_block, "snapshot consumption does not disable IRQs"
    assert "__enable_irq()" in consume_block, "snapshot consumption does not re-enable IRQs"
    assert "pwm_state.dirty = 0U;" in consume_block, "snapshot consumption does not clear dirty"

    apply_block = extract_function_body(
        my_application_c,
        r"static\s+void\s+PwmControl_ApplySnapshot\s*\(\s*const\s+PwmControlSnapshot\s*\*\s*snapshot\s*\)",
    )
    require(
        r"snapshot->lcd1_duty.*snapshot->led1_duty.*snapshot->lcd2_duty.*snapshot->led2_duty",
        apply_block,
        "PwmControl_ApplySnapshot does not use the local snapshot values",
    )
    if "pwm_state.dirty" in apply_block or "pwm_state.lcd1_duty" in apply_block:
        raise AssertionError("PwmControl_ApplySnapshot still reads or clears shared pwm_state fields")
    if re.search(r"static\s+void\s+PwmControl_Apply\s*\(", my_application_c):
        raise AssertionError("legacy PwmControl_Apply helper still exists")

    print("pwm control path checks passed")


if __name__ == "__main__":
    main()
