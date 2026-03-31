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
  uint8_t rx_buffer[PWM_CONTROL_PACKET_BYTES];
  uint8_t rx_count;
} PwmControlParserState;

typedef struct
{
  volatile uint8_t lcd1_duty;
  volatile uint8_t led1_duty;
  volatile uint8_t lcd2_duty;
  volatile uint8_t led2_duty;
  volatile uint8_t dirty;
  PwmControlParserState parser;
} PwmDutyState;

void PwmControl_Init(PwmDutyState *state);
void PwmControl_FeedBytes(PwmDutyState *state, const uint8_t *data, uint32_t len);

#endif
