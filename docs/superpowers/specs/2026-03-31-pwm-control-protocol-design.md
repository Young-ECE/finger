# PWM Control Protocol Design

## Goal

Restore PWM duty control alongside the existing 48 kHz dual-channel audio stream without disturbing the current host audio capture pipeline.

## Current State

The firmware currently exposes a one-way binary audio stream over USB CDC:

- the STM32 continuously sends 10 ms dual-channel audio packets to the host
- the host parses those packets and reconstructs stereo PCM
- USB OUT data is accepted by the CDC layer but ignored by the application
- TIM2 PWM output and duty control commands are not present in the current branch

## Requirements

- keep the existing audio streaming packet format unchanged
- add PWM duty control back for four outputs
- avoid introducing mixed packet types into the host-side audio receive stream
- keep the control path lightweight so it does not meaningfully affect USB throughput
- update the host tooling so PWM can be controlled from the same supported host-side workflow

## Recommended Approach

Use the current USB CDC connection in full duplex mode:

- device-to-host remains dedicated to audio stream packets
- host-to-device carries PWM control packets only
- the device does not emit PWM acknowledgements or status packets

This keeps the audio receive parser simple and avoids interleaving control replies into the host-side audio stream.

## Audio Stream Contract

The current audio transport remains unchanged:

- magic: `0xAA 0x55`
- version: `1`
- packet header: 16 bytes
- payload: 10 ms of interleaved stereo int16 PCM
- sample rate: 48 kHz
- samples per channel: 480

No host changes should be required to continue receiving audio beyond adding optional PWM control commands on the transmit side.

## PWM Control Packet

The host-to-device PWM command uses a new compact binary packet with a distinct magic value so it cannot be confused with audio stream data:

- byte 0-1: magic `0x43 0x54` (`'C' 'T'`)
- byte 2: protocol version `0x01`
- byte 3: packet type `0x01` (`SET_PWM_DUTY`)
- byte 4: payload length `0x04`
- byte 5: command sequence `0x00..0xFF`
- byte 6: `lcd1_duty`
- byte 7: `led1_duty`
- byte 8: `lcd2_duty`
- byte 9: `led2_duty`
- byte 10: CRC-8 over bytes `2..9`

Total packet size is 11 bytes.

## Duty Semantics

- each duty field is an unsigned percentage-like value in the range `0..99`
- values above `99` are clamped to `99`
- values below `0` are not representable in the wire format
- applying a valid packet immediately updates all four PWM compare registers atomically from the application point of view

The four duty fields map to the original output order:

- `lcd1_duty`
- `led1_duty`
- `lcd2_duty`
- `led2_duty`

## Firmware Design

### Peripheral Restoration

Reintroduce TIM2 PWM exactly for the four original outputs:

- TIM2 channel 1
- TIM2 channel 2
- TIM2 channel 3
- TIM2 channel 4

The generated GPIO and timer configuration should match the historical PA0-PA3 mapping used by the earlier local project version.

### USB Receive Path

The CDC receive callback will no longer ignore host data.

Instead it will:

- feed incoming bytes into a tiny control-packet parser
- search for the control magic `0x43 0x54`
- validate version, type, payload length, and CRC-8
- update a small in-memory PWM state structure when a valid packet is received
- ignore malformed packets without affecting audio transmission

### Runtime Application of Duty Values

The foreground loop will apply the latest accepted duty values to TIM2 with:

- `TIM_CHANNEL_1`
- `TIM_CHANNEL_2`
- `TIM_CHANNEL_3`
- `TIM_CHANNEL_4`

Audio streaming remains the higher-throughput responsibility; PWM writes are tiny register updates and should happen opportunistically in the main loop.

## Host Tooling Design

The supported host entry point remains `python_scripts/audio_stream_host.py`.

It will gain optional PWM command capabilities:

- send one PWM control packet before capture starts if explicit duty arguments are provided
- optionally allow PWM-only operation without requiring audio capture duration
- keep the current audio receive path unchanged

Recommended initial CLI shape:

- `--lcd1-duty`
- `--led1-duty`
- `--lcd2-duty`
- `--led2-duty`

If none of those arguments are provided, the tool behaves exactly like the current audio capture tool.

## Error Handling

Malformed control packets should be ignored safely.

The firmware should not:

- stop audio streaming
- emit debug text into the audio stream
- send ACK packets into the host audio receive path

The host should validate outgoing duty values before transmission and fail early on invalid input ranges.

## Bandwidth Expectations

The audio stream is currently about 193.6 KB/s.

The PWM control packet is only 11 bytes and travels in the opposite USB direction. Even repeated manual updates are negligible compared with the audio stream load, so this design should not create a meaningful USB bandwidth problem.

## Verification Plan

Verification should cover:

- valid PWM control packets change all four outputs as expected
- malformed control packets do not break audio streaming
- audio packets remain parseable by the current host parser after PWM support is added
- host CLI can send PWM values and still capture audio in the same session
- repeated PWM updates do not cause the audio packet drop counter to climb unexpectedly

## Non-Goals

This design does not add:

- PWM status or ACK packets from device to host
- runtime querying of current duty values from the device
- a GUI for PWM control
- changes to the audio packet format
