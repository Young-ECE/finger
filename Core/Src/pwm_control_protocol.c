#include "pwm_control_protocol.h"

static uint8_t pwm_rx_buffer[PWM_CONTROL_PACKET_BYTES];
static uint8_t pwm_rx_count;

static uint8_t PwmControl_Crc8(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0U;
  uint16_t index;
  uint8_t bit;

  for (index = 0U; index < len; ++index)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; ++bit)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1U) ^ 0x07U);
      }
      else
      {
        crc = (uint8_t)(crc << 1U);
      }
    }
  }

  return crc;
}

static uint8_t PwmControl_ClampDuty(uint8_t duty)
{
  return (duty > 99U) ? 99U : duty;
}

static void PwmControl_ResetParser(void)
{
  pwm_rx_count = 0U;
}

static void PwmControl_ApplyPacket(PwmDutyState *state, const uint8_t *packet)
{
  if ((packet[0] != PWM_CONTROL_MAGIC0) ||
      (packet[1] != PWM_CONTROL_MAGIC1) ||
      (packet[2] != PWM_CONTROL_PROTOCOL_VERSION) ||
      (packet[3] != PWM_CONTROL_TYPE_SET_DUTY) ||
      (packet[4] != PWM_CONTROL_PAYLOAD_LENGTH))
  {
    return;
  }

  if (PwmControl_Crc8(&packet[2], 8U) != packet[10])
  {
    return;
  }

  state->lcd1_duty = PwmControl_ClampDuty(packet[6]);
  state->led1_duty = PwmControl_ClampDuty(packet[7]);
  state->lcd2_duty = PwmControl_ClampDuty(packet[8]);
  state->led2_duty = PwmControl_ClampDuty(packet[9]);
  state->dirty = 1U;
}

static void PwmControl_PushByte(PwmDutyState *state, uint8_t byte)
{
  if (pwm_rx_count == 0U)
  {
    if (byte == PWM_CONTROL_MAGIC0)
    {
      pwm_rx_buffer[0] = byte;
      pwm_rx_count = 1U;
    }
    return;
  }

  if (pwm_rx_count == 1U)
  {
    if (byte == PWM_CONTROL_MAGIC1)
    {
      pwm_rx_buffer[1] = byte;
      pwm_rx_count = 2U;
    }
    else if (byte == PWM_CONTROL_MAGIC0)
    {
      pwm_rx_buffer[0] = byte;
      pwm_rx_count = 1U;
    }
    else
    {
      PwmControl_ResetParser();
    }
    return;
  }

  pwm_rx_buffer[pwm_rx_count++] = byte;
  if (pwm_rx_count >= PWM_CONTROL_PACKET_BYTES)
  {
    PwmControl_ApplyPacket(state, pwm_rx_buffer);
    if (byte == PWM_CONTROL_MAGIC0)
    {
      pwm_rx_buffer[0] = PWM_CONTROL_MAGIC0;
      pwm_rx_count = 1U;
    }
    else
    {
      PwmControl_ResetParser();
    }
  }
}

void PwmControl_Init(PwmDutyState *state)
{
  state->lcd1_duty = 0U;
  state->led1_duty = 0U;
  state->lcd2_duty = 0U;
  state->led2_duty = 0U;
  state->dirty = 1U;
  PwmControl_ResetParser();
}

void PwmControl_FeedBytes(PwmDutyState *state, const uint8_t *data, uint32_t len)
{
  uint32_t index;

  for (index = 0U; index < len; ++index)
  {
    PwmControl_PushByte(state, data[index]);
  }
}
