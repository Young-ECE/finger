# PWM Control Restoration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore four-channel PWM duty control over USB while keeping the existing 48 kHz dual-channel audio stream protocol and host capture workflow intact.

**Architecture:** Keep device-to-host USB CDC dedicated to the current audio packets and add a lightweight host-to-device control packet for PWM updates. Restore TIM2 PWM on PA0-PA3, parse control packets in the USB receive path, apply duty values from the foreground loop, and extend the supported Python host tool so it can both capture audio and send duty commands.

**Tech Stack:** STM32 HAL TIM/USB CDC/I2S in C, CubeMX-generated peripheral files, Python 3 with `unittest`, `numpy`, and `pyserial`

---

### Task 1: Define the PWM Control Packet in Shared Host-Side Tests and Helpers

**Files:**
- Create: `python_scripts/pwm_control_protocol.py`
- Create: `python_scripts/tests/test_pwm_control_protocol.py`
- Modify: `python_scripts/tests/test_audio_stream_host.py`
- Test: `python -m unittest python_scripts.tests.test_pwm_control_protocol -v`

- [ ] **Step 1: Write the failing host-side protocol tests**

```python
import unittest


class PwmControlProtocolTest(unittest.TestCase):
    def test_build_pwm_packet_matches_wire_format(self):
        from python_scripts.pwm_control_protocol import build_pwm_packet

        packet = build_pwm_packet(sequence=0x12, lcd1=10, led1=20, lcd2=30, led2=40)

        self.assertEqual(packet[:6], bytes([0x43, 0x54, 0x01, 0x01, 0x04, 0x12]))
        self.assertEqual(packet[6:10], bytes([10, 20, 30, 40]))
        self.assertEqual(len(packet), 11)

    def test_build_pwm_packet_rejects_out_of_range_values(self):
        from python_scripts.pwm_control_protocol import build_pwm_packet

        with self.assertRaises(ValueError):
            build_pwm_packet(sequence=0, lcd1=100, led1=0, lcd2=0, led2=0)
```

- [ ] **Step 2: Run the protocol tests to verify they fail**

Run: `python -m unittest python_scripts.tests.test_pwm_control_protocol -v`  
Expected: FAIL with `ModuleNotFoundError: No module named 'python_scripts.pwm_control_protocol'`

- [ ] **Step 3: Implement the host-side PWM protocol helper**

```python
from dataclasses import dataclass


MAGIC = b"\x43\x54"
PROTOCOL_VERSION = 1
PACKET_TYPE_SET_PWM_DUTY = 1
PAYLOAD_LENGTH = 4
PACKET_LENGTH = 11


def crc8(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def _validate_duty(name: str, value: int) -> int:
    if not 0 <= int(value) <= 99:
        raise ValueError(f"{name} must be in range 0..99, got {value}")
    return int(value)


@dataclass(frozen=True)
class PwmDutyCommand:
    sequence: int
    lcd1: int
    led1: int
    lcd2: int
    led2: int

    def to_bytes(self) -> bytes:
        body = bytes(
            [
                PROTOCOL_VERSION,
                PACKET_TYPE_SET_PWM_DUTY,
                PAYLOAD_LENGTH,
                self.sequence & 0xFF,
                _validate_duty("lcd1", self.lcd1),
                _validate_duty("led1", self.led1),
                _validate_duty("lcd2", self.lcd2),
                _validate_duty("led2", self.led2),
            ]
        )
        return MAGIC + body + bytes([crc8(body)])


def build_pwm_packet(sequence: int, lcd1: int, led1: int, lcd2: int, led2: int) -> bytes:
    return PwmDutyCommand(sequence=sequence, lcd1=lcd1, led1=led1, lcd2=lcd2, led2=led2).to_bytes()
```

- [ ] **Step 4: Re-run the protocol tests to verify they pass**

Run: `python -m unittest python_scripts.tests.test_pwm_control_protocol -v`  
Expected: PASS with both `test_build_pwm_packet_matches_wire_format` and `test_build_pwm_packet_rejects_out_of_range_values`

- [ ] **Step 5: Commit the shared protocol helper**

```bash
git add python_scripts/pwm_control_protocol.py python_scripts/tests/test_pwm_control_protocol.py
git commit -m "feat: add pwm control packet helper"
```

### Task 2: Restore TIM2 PWM Configuration and Static Wiring Checks

**Files:**
- Create: `python_scripts/tests/check_pwm_wiring.py`
- Create: `Core/Inc/tim.h`
- Create: `Core/Src/tim.c`
- Modify: `finger.ioc`
- Modify: `Core/Inc/main.h`
- Modify: `Core/Src/main.c`
- Test: `python python_scripts/tests/check_pwm_wiring.py`

- [ ] **Step 1: Write the failing static wiring check**

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

main_c = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8", errors="ignore")
main_h = (ROOT / "Core/Inc/main.h").read_text(encoding="utf-8", errors="ignore")
ioc = (ROOT / "finger.ioc").read_text(encoding="utf-8", errors="ignore")
tim_h = (ROOT / "Core/Inc/tim.h").read_text(encoding="utf-8", errors="ignore")
tim_c = (ROOT / "Core/Src/tim.c").read_text(encoding="utf-8", errors="ignore")

assert "MX_TIM2_Init();" in main_c, "main.c does not initialize TIM2"
assert "#include \"tim.h\"" in main_c, "main.c does not include tim.h"
assert "HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);" in main_c or "HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);" in tim_c, "PWM channel 1 is not started"
assert "TIM2" in ioc, "finger.ioc does not include TIM2"
assert "LCD1_Pin" in main_h and "LED2_Pin" in main_h, "main.h is missing PWM pin defines"
assert "extern TIM_HandleTypeDef htim2;" in tim_h, "tim.h does not export htim2"
assert "HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4)" in tim_c, "tim.c does not configure all four PWM channels"

print("pwm wiring checks passed")
```

- [ ] **Step 2: Run the wiring check to verify it fails**

Run: `python python_scripts/tests/check_pwm_wiring.py`  
Expected: FAIL with `main.c does not initialize TIM2` or `tim.h does not export htim2`

- [ ] **Step 3: Restore TIM2 in CubeMX and regenerate code**

```text
CubeMX:
- Enable TIM2 in PWM Generation CH1, CH2, CH3, and CH4 mode
- Map TIM2 CH1-CH4 to PA0, PA1, PA2, and PA3
- Keep Prescaler = 71 and Period = 99
- Generate code so the repo gains Core/Inc/tim.h, Core/Src/tim.c, TIM2 pin defines in Core/Inc/main.h, and MX_TIM2_Init() plus #include "tim.h" in Core/Src/main.c
```

```c
/* Core/Src/main.c */
#include "tim.h"

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  PeriphCommonClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_I2S1_Init();
  MX_USB_DEVICE_Init();
  MX_I2S2_Init();
  MX_TIM2_Init();

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  My_Application_Init();
  My_Application_Run();
}
```

- [ ] **Step 4: Re-run the wiring check to verify it passes**

Run: `python python_scripts/tests/check_pwm_wiring.py`  
Expected: PASS with `pwm wiring checks passed`

- [ ] **Step 5: Commit the restored TIM2 baseline**

```bash
git add finger.ioc Core/Inc/main.h Core/Inc/tim.h Core/Src/main.c Core/Src/tim.c python_scripts/tests/check_pwm_wiring.py
git commit -m "feat: restore tim2 pwm wiring"
```

### Task 3: Implement the Firmware-Side PWM Control Parser and Duty Application

**Files:**
- Create: `Core/Inc/pwm_control_protocol.h`
- Create: `Core/Src/pwm_control_protocol.c`
- Modify: `Core/Inc/my_application.h`
- Modify: `Core/Src/my_application.c`
- Modify: `USB_DEVICE/App/usbd_cdc_if.c`
- Modify: `python_scripts/tests/check_pwm_wiring.py`
- Test: `python python_scripts/tests/check_pwm_wiring.py`
- Test: `python python_scripts/tests/check_stream_wiring.py`

- [ ] **Step 1: Add a failing static assertion for the new receive path**

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

my_application_h = (ROOT / "Core/Inc/my_application.h").read_text(encoding="utf-8", errors="ignore")
my_application_c = (ROOT / "Core/Src/my_application.c").read_text(encoding="utf-8", errors="ignore")
cdc_if = (ROOT / "USB_DEVICE/App/usbd_cdc_if.c").read_text(encoding="utf-8", errors="ignore")

assert "void My_Application_OnUsbReceived(uint8_t *data, uint32_t len);" in my_application_h, "application USB receive hook missing"
assert "PwmControl_FeedBytes(" in cdc_if or "PwmControl_FeedBytes(" in my_application_c, "PWM control parser is not wired into USB receive handling"
assert "PwmControl_FeedBytes(" in my_application_c, "my_application.c does not parse PWM control bytes"
assert "__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4" in my_application_c, "my_application.c does not apply PWM duty"
assert "My_Application_OnUsbReceived(Buf, *Len);" in cdc_if, "CDC receive path does not forward data to the application"
```

- [ ] **Step 2: Run the check to verify it fails**

Run: `python python_scripts/tests/check_pwm_wiring.py`  
Expected: FAIL because the application hook and TIM2 duty writes do not exist yet

- [ ] **Step 3: Implement a focused PWM parser module and hook it into the application**

```c
/* Core/Inc/pwm_control_protocol.h */
#ifndef PWM_CONTROL_PROTOCOL_H
#define PWM_CONTROL_PROTOCOL_H

#include <stdint.h>

#define PWM_CONTROL_MAGIC0 0x43U
#define PWM_CONTROL_MAGIC1 0x54U
#define PWM_CONTROL_PROTOCOL_VERSION 1U
#define PWM_CONTROL_TYPE_SET_DUTY 0x01U
#define PWM_CONTROL_PAYLOAD_LENGTH 4U
#define PWM_CONTROL_PACKET_BYTES 11U

typedef struct
{
  uint8_t lcd1_duty;
  uint8_t led1_duty;
  uint8_t lcd2_duty;
  uint8_t led2_duty;
  volatile uint8_t dirty;
} PwmDutyState;

void PwmControl_Init(PwmDutyState *state);
void PwmControl_FeedBytes(PwmDutyState *state, const uint8_t *data, uint32_t len);

#endif
```

```c
/* Core/Src/pwm_control_protocol.c */
#include "pwm_control_protocol.h"

static uint8_t pwm_rx_buffer[PWM_CONTROL_PACKET_BYTES];
static uint8_t pwm_rx_count;

static uint8_t PwmControl_Crc8(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0U;
  for (uint16_t i = 0U; i < len; ++i)
  {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
      crc = (crc & 0x80U) ? (uint8_t)((crc << 1U) ^ 0x07U) : (uint8_t)(crc << 1U);
    }
  }
  return crc;
}

void PwmControl_Init(PwmDutyState *state)
{
  state->lcd1_duty = 0U;
  state->led1_duty = 0U;
  state->lcd2_duty = 0U;
  state->led2_duty = 0U;
  state->dirty = 1U;
  pwm_rx_count = 0U;
}
```

```c
/* Core/Src/my_application.c */
#include "pwm_control_protocol.h"
#include "tim.h"

extern TIM_HandleTypeDef htim2;
static PwmDutyState pwm_state;

void My_Application_OnUsbReceived(uint8_t *data, uint32_t len)
{
  PwmControl_FeedBytes(&pwm_state, data, len);
}

static void PwmControl_Apply(void)
{
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_state.lcd1_duty);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pwm_state.led1_duty);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pwm_state.lcd2_duty);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, pwm_state.led2_duty);
  pwm_state.dirty = 0U;
}

void My_Application_Init(void)
{
  AudioTx_Reset();
  PwmControl_Init(&pwm_state);
  RGB_LED_Init();
  HAL_Delay(100U);
  MIC_Init(&mic, &hi2s1);
  MIC_Init(&mic_2, &hi2s2);
  MIC_Start(&mic, (uint16_t *)dma_buffer);
  MIC_Start(&mic_2, (uint16_t *)dma_buffer_2);
  HAL_Delay(100U);
}

void My_Application_Run(void)
{
  while (1)
  {
    if (pwm_state.dirty != 0U)
    {
      PwmControl_Apply();
    }

    if ((mic.half_ready != 0U) && (mic_2.half_ready != 0U))
    {
      mic.half_ready = 0U;
      mic_2.half_ready = 0U;
      AudioTx_QueueBlock(&dma_buffer[0], &dma_buffer_2[0], AUDIO_FLAG_HALF_BLOCK);
    }

    if ((mic.full_ready != 0U) && (mic_2.full_ready != 0U))
    {
      mic.full_ready = 0U;
      mic_2.full_ready = 0U;
      AudioTx_QueueBlock(&dma_buffer[AUDIO_STREAM_DMA_WORDS_PER_BLOCK],
                         &dma_buffer_2[AUDIO_STREAM_DMA_WORDS_PER_BLOCK],
                         AUDIO_FLAG_FULL_BLOCK);
    }

    AudioTx_Process();
  }
}
```

```c
/* USB_DEVICE/App/usbd_cdc_if.c */
/* USER CODE BEGIN INCLUDE */
#include "my_application.h"
/* USER CODE END INCLUDE */

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  My_Application_OnUsbReceived(Buf, *Len);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}
```

- [ ] **Step 4: Re-run the static checks to verify the parser wiring is in place**

Run: `python python_scripts/tests/check_pwm_wiring.py && python python_scripts/tests/check_stream_wiring.py`  
Expected: PASS with `pwm wiring checks passed` and `stream wiring checks passed`

- [ ] **Step 5: Commit the firmware control path**

```bash
git add Core/Inc/pwm_control_protocol.h Core/Src/pwm_control_protocol.c Core/Inc/my_application.h Core/Src/my_application.c USB_DEVICE/App/usbd_cdc_if.c
git commit -m "feat: add firmware pwm control parser"
```

### Task 4: Extend the Supported Host Tool to Send PWM Commands Without Breaking Audio Capture

**Files:**
- Modify: `python_scripts/audio_stream_host.py`
- Modify: `python_scripts/tests/test_audio_stream_host.py`
- Test: `python -m unittest python_scripts.tests.test_audio_stream_host python_scripts.tests.test_pwm_control_protocol -v`

- [ ] **Step 1: Add a failing host test for PWM CLI behavior**

```python
import unittest
from argparse import Namespace


class AudioStreamHostPwmTest(unittest.TestCase):
    def test_cli_accepts_pwm_duty_arguments(self):
        from python_scripts.audio_stream_host import build_arg_parser

        args = build_arg_parser().parse_args(
            [
                "--port", "COM5",
                "--lcd1-duty", "10",
                "--led1-duty", "20",
                "--lcd2-duty", "30",
                "--led2-duty", "40",
                "--max-packets", "1",
            ]
        )

        self.assertEqual(args.lcd1_duty, 10)
        self.assertEqual(args.led2_duty, 40)
```

- [ ] **Step 2: Run the host tests to verify they fail**

Run: `python -m unittest python_scripts.tests.test_audio_stream_host -v`  
Expected: FAIL with `unrecognized arguments: --lcd1-duty 10`

- [ ] **Step 3: Update the CLI to send PWM packets before capture**

```python
from python_scripts.pwm_control_protocol import build_pwm_packet


def _requested_pwm_values(args) -> tuple[int, int, int, int] | None:
    values = (args.lcd1_duty, args.led1_duty, args.lcd2_duty, args.led2_duty)
    if all(value is None for value in values):
        return None
    if any(value is None for value in values):
        raise ValueError("provide all four PWM duty values together")
    return tuple(int(value) for value in values)


def send_pwm_command(serial_port, duties: tuple[int, int, int, int], sequence: int = 0) -> None:
    packet = build_pwm_packet(sequence=sequence, lcd1=duties[0], led1=duties[1], lcd2=duties[2], led2=duties[3])
    serial_port.write(packet)
    serial_port.flush()


def capture_stream(
    port: str,
    baudrate: int = DEFAULT_BAUDRATE,
    duration: float | None = None,
    max_packets: int | None = None,
    quiet: bool = False,
    pwm_duties: tuple[int, int, int, int] | None = None,
):
    with serial.Serial(port, baudrate, timeout=0.1) as serial_port:
        if pwm_duties is not None:
            send_pwm_command(serial_port, pwm_duties)
        while True:
            chunk = serial_port.read(4096)
            if chunk:
                for packet in parser.feed(chunk):
                    session.handle_packet(packet)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture the binary dual-channel audio stream, save WAV, and optionally send PWM duty commands."
    )
    parser.add_argument("--port", required=True, help="Serial port, for example COM5")
    parser.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE, help="Serial baud rate")
    parser.add_argument("--lcd1-duty", type=int, default=None, help="Optional LCD1 PWM duty in range 0..99")
    parser.add_argument("--led1-duty", type=int, default=None, help="Optional LED1 PWM duty in range 0..99")
    parser.add_argument("--lcd2-duty", type=int, default=None, help="Optional LCD2 PWM duty in range 0..99")
    parser.add_argument("--led2-duty", type=int, default=None, help="Optional LED2 PWM duty in range 0..99")
```

- [ ] **Step 4: Re-run the host tests to verify PWM control and audio capture coexist**

Run: `python -m unittest python_scripts.tests.test_audio_stream_host python_scripts.tests.test_pwm_control_protocol -v`  
Expected: PASS with the new PWM CLI test green and the existing audio host tests still green

- [ ] **Step 5: Run the full supported verification set**

Run: `python -m unittest python_scripts.tests.test_audio_stream_protocol python_scripts.tests.test_audio_feature_pipeline python_scripts.tests.test_audio_stream_host python_scripts.tests.test_pwm_control_protocol -v`  
Expected: PASS

Run: `python python_scripts/tests/check_audio_config.py`  
Expected: PASS with `audio config checks passed`

Run: `python python_scripts/tests/check_stream_wiring.py`  
Expected: PASS with `stream wiring checks passed`

Run: `python python_scripts/tests/check_pwm_wiring.py`  
Expected: PASS with `pwm wiring checks passed`

- [ ] **Step 6: Commit the host integration**

```bash
git add python_scripts/audio_stream_host.py python_scripts/tests/test_audio_stream_host.py python_scripts/pwm_control_protocol.py python_scripts/tests/test_pwm_control_protocol.py python_scripts/tests/check_pwm_wiring.py docs/superpowers/specs/2026-03-31-pwm-control-protocol-design.md
git commit -m "feat: restore pwm control over usb"
```
