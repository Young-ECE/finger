# 48 kHz Audio Streaming Alignment Design

## Goal

Align the firmware and host pipeline with robot audio workflows such as ManiWAV, Hearing Touch, and PolyTouch:

- capture continuous dual-channel audio on the STM32 at 48 kHz
- stream audio blocks to the host instead of single latest-value samples
- resample on the host to 16 kHz
- compute log-mel features on the host
- align audio features with 60 Hz image frames on the host

## Current State

The current firmware already configures both I2S peripherals for 48 kHz sampling, but the data path is still "latest sample only":

- DMA callbacks extract one sample from each completed DMA block
- the application stores that sample in `audio_result_left`
- USB sends a tiny status packet containing two latest microphone values

This is useful for monitoring, but it is not sufficient for continuous audio feature extraction.

## Target Architecture

### Firmware Responsibilities

The STM32 firmware will:

- keep both microphones sampling continuously at 48 kHz
- keep using DMA and half/full completion callbacks
- replace single-value output with block-based output
- convert microphone samples to signed 16-bit PCM for transport
- send fixed-duration dual-channel audio packets over USB CDC
- include a packet sequence number so the host can detect drops and rebuild a stable timeline

The firmware will not:

- resample to 16 kHz
- compute mel or log-mel features
- manage 60 Hz image synchronization directly

### Host Responsibilities

The host will:

- receive and reorder packetized audio blocks
- detect dropped or out-of-order packets by sequence number
- reconstruct a continuous 48 kHz stereo stream
- resample to 16 kHz
- generate log-mel spectrogram features
- align features to 60 Hz image frames

## Data Flow

1. I2S1 and I2S2 capture microphone data continuously at 48 kHz.
2. DMA half/full callbacks mark completed microphone blocks as ready.
3. A non-interrupt firmware path pairs left/right ready blocks.
4. The paired blocks are converted from microphone sample format to interleaved stereo int16 PCM.
5. The firmware builds a fixed-size USB audio packet and transmits it.
6. The host reconstructs the audio stream from packet sequence order.
7. The host resamples 48 kHz audio to 16 kHz.
8. The host computes log-mel features on a 60 Hz feature hop for image alignment.

## Packet Format

Each USB audio packet will contain:

- 2-byte magic
- 1-byte protocol version
- 1-byte flags
- 4-byte sequence number
- 2-byte sample rate (`48000`)
- 1-byte channel count (`2`)
- 1-byte bits per sample (`16`)
- 2-byte samples per channel
- 2-byte payload byte count
- payload: interleaved stereo int16 PCM (`L0, R0, L1, R1, ...`)

The sequence number is mandatory. It is the simplest way to diagnose packet loss and align host-side audio and image timelines.

## Block Size and Timing

### Recommended Transport Block

Use a 10 ms audio packet duration.

At 48 kHz this means:

- 480 samples per channel per packet
- 960 total samples across two channels
- 1920 bytes of PCM payload per packet

This is the recommended balance between:

- manageable USB transaction rate
- low latency
- easy host-side reconstruction
- compatibility with the current `APP_TX_DATA_SIZE = 2048`

### Why Not Force 60 Hz Packets

Do not force the firmware to emit one packet per image frame.

A 60 Hz cadence would imply 16.67 ms packets:

- 800 samples per channel
- 3200 bytes of stereo int16 PCM payload

That is less convenient for the current CDC buffer sizing and would unnecessarily couple firmware transport timing to camera timing. The host should perform the 60 Hz alignment instead.

## Firmware Design

### Sampling

- Keep both I2S peripherals at 48 kHz.
- Keep the existing DMA circular mode.
- Preserve interrupt work as lightweight block-ready signaling only.

### DMA Callback Behavior

The DMA callbacks will no longer extract one latest sample into `audio_result_left`.

Instead they will:

- identify which half of which DMA buffer has completed
- mark that half-buffer as ready for transport
- avoid USB transmission from interrupt context

### Audio Block Assembly

A foreground transport path will:

- wait until both microphone channels have a matching ready block
- read the completed block from each channel
- convert each microphone sample to int16 PCM
- interleave the two channels
- populate packet headers
- enqueue the completed USB packet into a transmit ring buffer

### USB Sending Strategy

USB transmission must become queue-based rather than single-packet opportunistic sending.

The firmware should maintain a 4-packet transmit ring so that:

- DMA completion does not block on USB state
- temporary `USBD_BUSY` conditions do not immediately discard audio

If the queue overflows, the firmware must increment a drop counter and set a status flag. Silent packet loss is not acceptable.

## Host Pipeline Design

### Stream Reconstruction

The host receiver will:

- parse packet headers
- verify packet shape and payload size
- track sequence numbers
- insert gaps or mark discontinuities if packets are lost

### Feature Pipeline

Recommended host processing:

- input stream: stereo 48 kHz int16 PCM
- resample: 48 kHz to 16 kHz
- feature window: 25 ms
- feature hop: 16.67 ms to align with 60 Hz images
- mel bins: 64 or 80
- output: log-mel spectrogram frames for downstream AST or multimodal models

The firmware remains independent from these feature parameters so the host can tune them without reflashing the MCU.

## Error Handling and Observability

The design should expose enough state to debug transport quality:

- packet sequence number
- transport overflow counter
- USB busy counter
- firmware status flags in the packet header

This information is useful when validating packet stability at 48 kHz and when correlating missing audio with host-side feature gaps.

## Verification Plan

Firmware and host validation should cover:

- both I2S peripherals remain stable at 48 kHz
- dual-channel packets arrive continuously on the host
- packet sequence numbers increase monotonically without unexplained gaps
- host reconstruction produces a valid stereo waveform
- resampling to 16 kHz succeeds without shape mismatches
- log-mel generation produces frames at a 60 Hz hop
- image/audio alignment remains stable over multi-second captures

## Non-Goals

This design does not attempt to:

- compute mel features on the STM32
- bind USB packet rate to camera frame rate
- preserve backward compatibility with the old 16-byte latest-value packet

## Implementation Scope

The implementation will require changes in:

- DMA callback data handling
- microphone transport buffering
- USB packet construction and send path
- host-side packet parser and audio reconstruction logic

It does not require changing the chosen 48 kHz CubeMX clock configuration unless testing reveals instability.
