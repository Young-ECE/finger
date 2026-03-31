# Audio Stream Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current latest-sample CSV microphone output with a 48 kHz dual-channel binary audio stream that the host can reconstruct, resample to 16 kHz, and turn into 60 Hz log-mel features.

**Architecture:** Keep both I2S peripherals sampling continuously at 48 kHz, but move from single-value microphone state to fixed-size DMA-backed audio blocks. Convert each completed dual-channel block to interleaved int16 PCM, send it over USB CDC in a binary packet with a sequence number, and add host-side Python modules that parse the stream, rebuild the timeline, resample to 16 kHz, and compute log-mel features.

**Tech Stack:** STM32 HAL I2S/DMA/USB CDC in C, CubeMX-generated clock config, Python 3 with `numpy`, `pyserial`, and `scipy`

---

### Task 1: Lock 48 kHz Audio Configuration and Shared Constants

**Files:**
- Create: `Core/Inc/audio_stream_transport.h`
- Create: `python_scripts/tests/check_audio_config.py`
- Modify: `Core/Inc/microphone_sensor.h`
- Modify: `Core/Src/i2s.c`
- Modify: `Core/Src/main.c`
- Modify: `finger.ioc`
- Test: `python_scripts/tests/check_audio_config.py`

- [ ] **Step 1: Write the failing configuration check**

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

i2s_c = (ROOT / "Core/Src/i2s.c").read_text(encoding="utf-8", errors="ignore")
main_c = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8", errors="ignore")
mic_h = (ROOT / "Core/Inc/microphone_sensor.h").read_text(encoding="utf-8", errors="ignore")
ioc = (ROOT / "finger.ioc").read_text(encoding="utf-8", errors="ignore")

assert "hi2s1.Init.AudioFreq = I2S_AUDIOFREQ_48K;" in i2s_c, "hi2s1 is not 48 kHz"
assert "hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;" in i2s_c, "hi2s2 is not 48 kHz"
assert "PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;" in main_c, "PLLI2SN is not 192"
assert "PeriphClkInitStruct.PLLI2S.PLLI2SR = 5;" in main_c, "PLLI2SR is not 5"
assert "#define MIC_BUFFER_SIZE AUDIO_STREAM_DMA_BUFFER_WORDS" in mic_h, "MIC buffer does not use stream constants"
assert "I2S1.AudioFreq=I2S_AUDIOFREQ_48K" in ioc, "CubeMX I2S1 config is not 48 kHz"
assert "I2S2.AudioFreq=I2S_AUDIOFREQ_48K" in ioc, "CubeMX I2S2 config is not 48 kHz"

print("audio config checks passed")
```

- [ ] **Step 2: Run the check to verify it fails**

Run: `python python_scripts/tests/check_audio_config.py`  
Expected: FAIL with at least one of `hi2s1 is not 48 kHz`, `hi2s2 is not 48 kHz`, or `MIC buffer does not use stream constants`

- [ ] **Step 3: Add the shared transport constants and update the firmware configuration**

```c
/* Core/Inc/audio_stream_transport.h */
#ifndef AUDIO_STREAM_TRANSPORT_H
#define AUDIO_STREAM_TRANSPORT_H

#include <stdint.h>

#define AUDIO_STREAM_MAGIC0 0xAAU
#define AUDIO_STREAM_MAGIC1 0x55U
#define AUDIO_STREAM_PROTOCOL_VERSION 1U
#define AUDIO_STREAM_SAMPLE_RATE 48000U
#define AUDIO_STREAM_CHANNELS 2U
#define AUDIO_STREAM_BITS_PER_SAMPLE 16U
#define AUDIO_STREAM_SAMPLES_PER_PACKET 480U
#define AUDIO_STREAM_DMA_WORDS_PER_SAMPLE 2U
#define AUDIO_STREAM_DMA_WORDS_PER_BLOCK (AUDIO_STREAM_SAMPLES_PER_PACKET * AUDIO_STREAM_DMA_WORDS_PER_SAMPLE)
#define AUDIO_STREAM_DMA_BUFFER_WORDS (AUDIO_STREAM_DMA_WORDS_PER_BLOCK * 2U)
#define AUDIO_STREAM_HEADER_BYTES 16U
#define AUDIO_STREAM_PAYLOAD_SAMPLES (AUDIO_STREAM_SAMPLES_PER_PACKET * AUDIO_STREAM_CHANNELS)
#define AUDIO_STREAM_PAYLOAD_BYTES (AUDIO_STREAM_PAYLOAD_SAMPLES * sizeof(int16_t))
#define AUDIO_STREAM_PACKET_BYTES (AUDIO_STREAM_HEADER_BYTES + AUDIO_STREAM_PAYLOAD_BYTES)

int32_t AudioStream_ExtractRaw24(const uint32_t *buffer, uint16_t sample_index);
int16_t AudioStream_Raw24ToPcm16(int32_t raw24);
void AudioStream_InterleavePcmBlock(const uint32_t *left_buffer,
                                    const uint32_t *right_buffer,
                                    int16_t *dst_interleaved);
uint16_t AudioStream_BuildPacket(uint8_t *dst,
                                 uint32_t sequence,
                                 uint8_t flags,
                                 const int16_t *interleaved_pcm);

#endif
```

```c
/* Core/Inc/microphone_sensor.h */
#include "audio_stream_transport.h"

#define MIC_BUFFER_SIZE AUDIO_STREAM_DMA_BUFFER_WORDS

typedef struct
{
    I2S_HandleTypeDef *hi2s;
    volatile int32_t audio_result_left;
    volatile int32_t audio_result_right;
    volatile uint8_t half_ready;
    volatile uint8_t full_ready;
} MIC_HandleTypeDef;
```

```c
/* Core/Src/i2s.c */
hi2s1.Init.AudioFreq = I2S_AUDIOFREQ_48K;
...
hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;
```

```c
/* Core/Src/main.c */
PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;
PeriphClkInitStruct.PLLI2S.PLLI2SR = 5;
```

```ini
# finger.ioc
I2S1.AudioFreq=I2S_AUDIOFREQ_48K
I2S1.RealAudioFreq=48.0 KHz
I2S2.AudioFreq=I2S_AUDIOFREQ_48K
I2S2.RealAudioFreq=48.0 KHz
RCC.PLLI2SN=192
RCC.PLLI2SR=5
RCC.I2S1Freq_Value=76800000
RCC.I2S2Freq_Value=76800000
```

- [ ] **Step 4: Run the configuration check to verify it passes**

Run: `python python_scripts/tests/check_audio_config.py`  
Expected: PASS with `audio config checks passed`

- [ ] **Step 5: Build the firmware to verify the configuration changes compile**

Run: `make -j4`  
Expected: PASS with `build/finger.elf`, `build/finger.hex`, and `build/finger.bin` generated

- [ ] **Step 6: Commit the configuration baseline**

```bash
git add Core/Inc/audio_stream_transport.h Core/Inc/microphone_sensor.h Core/Src/i2s.c Core/Src/main.c finger.ioc python_scripts/tests/check_audio_config.py
git commit -m "chore: lock 48khz audio transport config"
```

### Task 2: Add Testable Audio Transport Helpers

**Files:**
- Create: `Core/Src/audio_stream_transport.c`
- Create: `tools/tests/test_audio_stream_transport.c`
- Modify: `Makefile`
- Modify: `Core/Inc/audio_stream_transport.h`
- Test: `tools/tests/test_audio_stream_transport.c`

- [ ] **Step 1: Write the failing host-side transport harness**

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio_stream_transport.h"

static void test_extract_and_convert(void)
{
    uint32_t words[AUDIO_STREAM_DMA_WORDS_PER_BLOCK] = {0};

    words[0] = 0x0004U;
    words[1] = 0x0000U;
    words[2] = 0xFFFCU;
    words[3] = 0x0000U;

    assert(AudioStream_ExtractRaw24(words, 0U) == 256);
    assert(AudioStream_Raw24ToPcm16(AudioStream_ExtractRaw24(words, 0U)) == 1);
    assert(AudioStream_Raw24ToPcm16(AudioStream_ExtractRaw24(words, 1U)) == -1);
}

static void test_packet_header(void)
{
    int16_t pcm[AUDIO_STREAM_PAYLOAD_SAMPLES] = {0};
    uint8_t packet[AUDIO_STREAM_PACKET_BYTES] = {0};
    uint16_t length = AudioStream_BuildPacket(packet, 7U, 0x03U, pcm);

    assert(length == AUDIO_STREAM_PACKET_BYTES);
    assert(packet[0] == AUDIO_STREAM_MAGIC0);
    assert(packet[1] == AUDIO_STREAM_MAGIC1);
    assert(packet[2] == AUDIO_STREAM_PROTOCOL_VERSION);
    assert(packet[3] == 0x03U);
    assert(packet[4] == 7U);
    assert(packet[8] == (uint8_t)(AUDIO_STREAM_SAMPLE_RATE & 0xFFU));
}

int main(void)
{
    test_extract_and_convert();
    test_packet_header();
    puts("audio_stream_transport tests passed");
    return 0;
}
```

- [ ] **Step 2: Run the harness build to verify it fails**

Run: `gcc -Wall -Wextra -ICore/Inc -o build/test_audio_stream_transport.exe tools/tests/test_audio_stream_transport.c Core/Src/audio_stream_transport.c`  
Expected: FAIL with `No such file or directory` for `Core/Src/audio_stream_transport.c` or unresolved symbols from `audio_stream_transport.h`

- [ ] **Step 3: Implement the transport helper module and add it to the firmware build**

```c
/* Core/Src/audio_stream_transport.c */
#include "audio_stream_transport.h"
#include <string.h>

static void AudioStream_WriteLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void AudioStream_WriteLe32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

int32_t AudioStream_ExtractRaw24(const uint32_t *buffer, uint16_t sample_index)
{
    uint32_t msw = buffer[sample_index * AUDIO_STREAM_DMA_WORDS_PER_SAMPLE] & 0xFFFFU;
    uint32_t lsw = buffer[sample_index * AUDIO_STREAM_DMA_WORDS_PER_SAMPLE + 1U] & 0xFFFFU;
    uint32_t packed = (msw << 16) | lsw;
    return ((int32_t)packed) >> 10;
}

int16_t AudioStream_Raw24ToPcm16(int32_t raw24)
{
    return (int16_t)(raw24 >> 8);
}

void AudioStream_InterleavePcmBlock(const uint32_t *left_buffer,
                                    const uint32_t *right_buffer,
                                    int16_t *dst_interleaved)
{
    for (uint16_t i = 0; i < AUDIO_STREAM_SAMPLES_PER_PACKET; ++i)
    {
        dst_interleaved[i * 2U] = AudioStream_Raw24ToPcm16(AudioStream_ExtractRaw24(left_buffer, i));
        dst_interleaved[i * 2U + 1U] = AudioStream_Raw24ToPcm16(AudioStream_ExtractRaw24(right_buffer, i));
    }
}

uint16_t AudioStream_BuildPacket(uint8_t *dst,
                                 uint32_t sequence,
                                 uint8_t flags,
                                 const int16_t *interleaved_pcm)
{
    dst[0] = AUDIO_STREAM_MAGIC0;
    dst[1] = AUDIO_STREAM_MAGIC1;
    dst[2] = AUDIO_STREAM_PROTOCOL_VERSION;
    dst[3] = flags;
    AudioStream_WriteLe32(&dst[4], sequence);
    AudioStream_WriteLe16(&dst[8], AUDIO_STREAM_SAMPLE_RATE);
    dst[10] = AUDIO_STREAM_CHANNELS;
    dst[11] = AUDIO_STREAM_BITS_PER_SAMPLE;
    AudioStream_WriteLe16(&dst[12], AUDIO_STREAM_SAMPLES_PER_PACKET);
    AudioStream_WriteLe16(&dst[14], AUDIO_STREAM_PAYLOAD_BYTES);
    memcpy(&dst[AUDIO_STREAM_HEADER_BYTES], interleaved_pcm, AUDIO_STREAM_PAYLOAD_BYTES);
    return AUDIO_STREAM_PACKET_BYTES;
}
```

```make
# Makefile
C_SOURCES = \
Core/Src/main.c \
... \
Core/Src/audio_stream_transport.c \
...
```

- [ ] **Step 4: Run the harness build and execute it**

Run: `gcc -Wall -Wextra -ICore/Inc -o build/test_audio_stream_transport.exe tools/tests/test_audio_stream_transport.c Core/Src/audio_stream_transport.c && .\\build\\test_audio_stream_transport.exe`  
Expected: PASS with `audio_stream_transport tests passed`

- [ ] **Step 5: Build the firmware to verify the new helper compiles in the embedded target**

Run: `make -j4`  
Expected: PASS with no missing-symbol errors for `AudioStream_*`

- [ ] **Step 6: Commit the transport helper**

```bash
git add Core/Inc/audio_stream_transport.h Core/Src/audio_stream_transport.c Makefile tools/tests/test_audio_stream_transport.c
git commit -m "feat: add testable audio stream transport helpers"
```

### Task 3: Replace Latest-Value CSV Output with Queued Binary Audio Packets

**Files:**
- Create: `python_scripts/tests/check_stream_wiring.py`
- Modify: `Core/Src/stm32f4xx_it.c`
- Modify: `Core/Src/my_application.c`
- Modify: `Core/Src/microphone_sensor.c`
- Modify: `Core/Inc/microphone_sensor.h`
- Test: `python_scripts/tests/check_stream_wiring.py`

- [ ] **Step 1: Write the failing wiring check**

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

it_c = (ROOT / "Core/Src/stm32f4xx_it.c").read_text(encoding="utf-8", errors="ignore")
my_app = (ROOT / "Core/Src/my_application.c").read_text(encoding="utf-8", errors="ignore")

assert "AudioStream_InterleavePcmBlock(" in my_app, "my_application.c is not building interleaved audio blocks"
assert "AudioStream_BuildPacket(" in my_app, "my_application.c is not building binary audio packets"
assert "CDC_Transmit_FS((uint8_t*)msg, len);" not in my_app, "legacy CSV transmit path is still present"
assert "mic.audio_result_left =" not in it_c, "DMA callbacks still overwrite a single latest sample"

print("stream wiring checks passed")
```

- [ ] **Step 2: Run the wiring check to verify it fails**

Run: `python python_scripts/tests/check_stream_wiring.py`  
Expected: FAIL with `my_application.c is not building interleaved audio blocks` or `legacy CSV transmit path is still present`

- [ ] **Step 3: Replace the DMA callback path with block-ready flags and add a 4-packet USB transmit ring**

```c
/* Core/Src/stm32f4xx_it.c */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s == &hi2s1)
  {
    mic.half_ready = 1U;
  }
  if (hi2s == &hi2s2)
  {
    mic_2.half_ready = 1U;
  }
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s == &hi2s1)
  {
    mic.full_ready = 1U;
  }
  if (hi2s == &hi2s2)
  {
    mic_2.full_ready = 1U;
  }
}
```

```c
/* Core/Src/my_application.c */
#include "audio_stream_transport.h"
#include "usbd_cdc_if.h"

#define AUDIO_TX_QUEUE_DEPTH 4U
#define AUDIO_FLAG_HALF_BLOCK 0x01U
#define AUDIO_FLAG_FULL_BLOCK 0x02U
#define AUDIO_FLAG_QUEUE_OVERFLOW 0x80U

typedef struct
{
  uint8_t bytes[AUDIO_STREAM_PACKET_BYTES];
  uint16_t length;
} AudioTxPacket;

static AudioTxPacket audio_tx_queue[AUDIO_TX_QUEUE_DEPTH];
static uint8_t audio_tx_head = 0U;
static uint8_t audio_tx_tail = 0U;
static uint32_t audio_sequence = 0U;
static uint32_t audio_overflow_count = 0U;
static uint32_t usb_busy_count = 0U;
static uint8_t audio_status_flags = 0U;
static int16_t audio_pcm_block[AUDIO_STREAM_PAYLOAD_SAMPLES];

static uint8_t AudioTx_QueueBlock(const uint32_t *left_block,
                                  const uint32_t *right_block,
                                  uint8_t flags)
{
  uint8_t next_tail = (uint8_t)((audio_tx_tail + 1U) % AUDIO_TX_QUEUE_DEPTH);
  if (next_tail == audio_tx_head)
  {
    audio_overflow_count++;
    audio_status_flags |= AUDIO_FLAG_QUEUE_OVERFLOW;
    return 0U;
  }

  AudioStream_InterleavePcmBlock(left_block, right_block, audio_pcm_block);
  audio_tx_queue[audio_tx_tail].length =
      AudioStream_BuildPacket(audio_tx_queue[audio_tx_tail].bytes,
                              audio_sequence++,
                              (uint8_t)(flags | audio_status_flags),
                              audio_pcm_block);
  audio_status_flags = 0U;
  audio_tx_tail = next_tail;
  return 1U;
}

static void AudioTx_Process(void)
{
  if (audio_tx_head == audio_tx_tail)
  {
    return;
  }

  if (CDC_Transmit_FS(audio_tx_queue[audio_tx_head].bytes, audio_tx_queue[audio_tx_head].length) == USBD_OK)
  {
    audio_tx_head = (uint8_t)((audio_tx_head + 1U) % AUDIO_TX_QUEUE_DEPTH);
  }
  else
  {
    usb_busy_count++;
  }
}
```

```c
/* Core/Src/my_application.c : inside My_Application_Run() */
  while (1)
  {
    if (mic.half_ready && mic_2.half_ready)
    {
      mic.half_ready = 0U;
      mic_2.half_ready = 0U;
      (void)AudioTx_QueueBlock(&dma_buffer[0], &dma_buffer_2[0], AUDIO_FLAG_HALF_BLOCK);
    }

    if (mic.full_ready && mic_2.full_ready)
    {
      mic.full_ready = 0U;
      mic_2.full_ready = 0U;
      (void)AudioTx_QueueBlock(&dma_buffer[AUDIO_STREAM_DMA_WORDS_PER_BLOCK],
                               &dma_buffer_2[AUDIO_STREAM_DMA_WORDS_PER_BLOCK],
                               AUDIO_FLAG_FULL_BLOCK);
    }

    AudioTx_Process();
  }
```

```c
/* Core/Src/microphone_sensor.c */
HAL_StatusTypeDef MIC_Start(MIC_HandleTypeDef *mic, uint16_t *buffer)
{
    if (!mic || !mic->hi2s) return HAL_ERROR;
    return HAL_I2S_Receive_DMA(mic->hi2s, (uint16_t *)buffer, MIC_BUFFER_SIZE / 2U);
}
```

Remove the legacy `sprintf(... mic.audio_result_left ...)` CSV transmit loop and the startup `CDC_Transmit_FS((uint8_t*)msg, ...)` banners so the CDC stream is binary-only once the device is running.

- [ ] **Step 4: Run the wiring check to verify it passes**

Run: `python python_scripts/tests/check_stream_wiring.py`  
Expected: PASS with `stream wiring checks passed`

- [ ] **Step 5: Build the firmware and smoke-check the binary packet path**

Run: `make -j4`  
Expected: PASS with no compile errors in `my_application.c`, `stm32f4xx_it.c`, or `microphone_sensor.c`

- [ ] **Step 6: Commit the firmware transport wiring**

```bash
git add Core/Src/stm32f4xx_it.c Core/Src/my_application.c Core/Src/microphone_sensor.c Core/Inc/microphone_sensor.h python_scripts/tests/check_stream_wiring.py
git commit -m "feat: stream queued binary audio packets over usb"
```

### Task 4: Add a Host-Side Binary Packet Parser and Continuous Stream Reconstructor

**Files:**
- Create: `python_scripts/audio_stream_protocol.py`
- Create: `python_scripts/tests/test_audio_stream_protocol.py`
- Test: `python_scripts/tests/test_audio_stream_protocol.py`

- [ ] **Step 1: Write the failing Python protocol test**

```python
import struct
import unittest

from python_scripts.audio_stream_protocol import AudioStreamParser

MAGIC = b"\xAA\x55"
HEADER = struct.Struct("<2sBBIHBBHH")


def build_packet(sequence, flags, samples_per_channel, interleaved_pcm):
    payload = struct.pack("<" + "h" * len(interleaved_pcm), *interleaved_pcm)
    header = HEADER.pack(
        MAGIC,
        1,
        flags,
        sequence,
        48000,
        2,
        16,
        samples_per_channel,
        len(payload),
    )
    return header + payload


class AudioStreamParserTest(unittest.TestCase):
    def test_parses_a_complete_packet(self):
        parser = AudioStreamParser()
        packet = build_packet(7, 0x01, 2, [1, -1, 2, -2])
        frames = parser.feed(packet)

        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].sequence, 7)
        self.assertEqual(frames[0].flags, 0x01)
        self.assertEqual(frames[0].pcm.tolist(), [[1, -1], [2, -2]])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the protocol test to verify it fails**

Run: `python -m unittest python_scripts.tests.test_audio_stream_protocol -v`  
Expected: FAIL with `ModuleNotFoundError: No module named 'python_scripts.audio_stream_protocol'`

- [ ] **Step 3: Implement the parser and reconstructor**

```python
from dataclasses import dataclass
import struct
import numpy as np

MAGIC = b"\xAA\x55"
HEADER = struct.Struct("<2sBBIHBBHH")


@dataclass
class AudioPacket:
    sequence: int
    flags: int
    sample_rate: int
    channels: int
    bits_per_sample: int
    samples_per_channel: int
    pcm: np.ndarray


class AudioStreamParser:
    def __init__(self):
        self._buffer = bytearray()

    def feed(self, chunk: bytes):
        self._buffer.extend(chunk)
        packets = []

        while True:
            if len(self._buffer) < HEADER.size:
                return packets

            magic_index = self._buffer.find(MAGIC)
            if magic_index < 0:
                self._buffer.clear()
                return packets
            if magic_index:
                del self._buffer[:magic_index]
            if len(self._buffer) < HEADER.size:
                return packets

            magic, version, flags, sequence, sample_rate, channels, bits_per_sample, samples_per_channel, payload_bytes = HEADER.unpack_from(self._buffer)
            packet_bytes = HEADER.size + payload_bytes
            if len(self._buffer) < packet_bytes:
                return packets

            payload = bytes(self._buffer[HEADER.size:packet_bytes])
            del self._buffer[:packet_bytes]

            pcm = np.frombuffer(payload, dtype="<i2").reshape(samples_per_channel, channels)
            packets.append(
                AudioPacket(
                    sequence=sequence,
                    flags=flags,
                    sample_rate=sample_rate,
                    channels=channels,
                    bits_per_sample=bits_per_sample,
                    samples_per_channel=samples_per_channel,
                    pcm=pcm,
                )
            )


class AudioStreamReconstructor:
    def __init__(self):
        self.expected_sequence = None
        self.dropped_packets = 0

    def push(self, packet: AudioPacket):
        if self.expected_sequence is not None and packet.sequence != self.expected_sequence:
            self.dropped_packets += packet.sequence - self.expected_sequence
        self.expected_sequence = packet.sequence + 1
        return packet.pcm
```

- [ ] **Step 4: Run the protocol test to verify it passes**

Run: `python -m unittest python_scripts.tests.test_audio_stream_protocol -v`  
Expected: PASS with `OK`

- [ ] **Step 5: Commit the host-side stream parser**

```bash
git add python_scripts/audio_stream_protocol.py python_scripts/tests/test_audio_stream_protocol.py
git commit -m "feat: add host-side audio stream parser"
```

### Task 5: Add Host-Side Resampling and Log-Mel Feature Extraction

**Files:**
- Create: `python_scripts/audio_feature_pipeline.py`
- Create: `python_scripts/tests/test_audio_feature_pipeline.py`
- Modify: `requirements.txt`
- Test: `python_scripts/tests/test_audio_feature_pipeline.py`

- [ ] **Step 1: Write the failing feature-pipeline test**

```python
import unittest
import numpy as np

from python_scripts.audio_feature_pipeline import resample_to_16k, compute_log_mel


class AudioFeaturePipelineTest(unittest.TestCase):
    def test_resample_and_log_mel_shape(self):
        t = np.arange(4800, dtype=np.float32) / 48000.0
        stereo = np.stack(
            [
                np.sin(2.0 * np.pi * 440.0 * t),
                np.sin(2.0 * np.pi * 660.0 * t),
            ],
            axis=1,
        )

        mono_16k = resample_to_16k(stereo, input_rate=48000)
        mel = compute_log_mel(mono_16k, sample_rate=16000, n_mels=64, hop_ms=(1000.0 / 60.0))

        self.assertEqual(mono_16k.shape[0], 1600)
        self.assertEqual(mel.shape[0], 64)
        self.assertGreater(mel.shape[1], 0)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the feature-pipeline test to verify it fails**

Run: `python -m unittest python_scripts.tests.test_audio_feature_pipeline -v`  
Expected: FAIL with `ModuleNotFoundError: No module named 'python_scripts.audio_feature_pipeline'`

- [ ] **Step 3: Add the feature pipeline and the one new dependency it needs**

```text
# requirements.txt
PySide6>=6.4.0
pyqtgraph>=0.13.0
pyserial>=3.5
numpy>=1.23.0
scipy>=1.10.0
```

```python
import numpy as np
from scipy.signal import resample_poly, stft


def stereo_to_mono(stereo: np.ndarray) -> np.ndarray:
    return stereo.astype(np.float32).mean(axis=1)


def resample_to_16k(stereo: np.ndarray, input_rate: int = 48000) -> np.ndarray:
    mono = stereo_to_mono(stereo)
    if input_rate == 16000:
        return mono
    if input_rate != 48000:
        raise ValueError(f"unsupported input_rate: {input_rate}")
    return resample_poly(mono, up=1, down=3).astype(np.float32)


def hz_to_mel(hz: np.ndarray) -> np.ndarray:
    return 2595.0 * np.log10(1.0 + hz / 700.0)


def mel_to_hz(mel: np.ndarray) -> np.ndarray:
    return 700.0 * (10.0 ** (mel / 2595.0) - 1.0)


def build_mel_filterbank(sample_rate: int, n_fft: int, n_mels: int) -> np.ndarray:
    mel_edges = np.linspace(hz_to_mel(np.array([0.0]))[0], hz_to_mel(np.array([sample_rate / 2.0]))[0], n_mels + 2)
    hz_edges = mel_to_hz(mel_edges)
    bins = np.floor((n_fft + 1) * hz_edges / sample_rate).astype(int)
    filters = np.zeros((n_mels, n_fft // 2 + 1), dtype=np.float32)

    for i in range(n_mels):
        left, center, right = bins[i], bins[i + 1], bins[i + 2]
        filters[i, left:center] = np.linspace(0.0, 1.0, max(center - left, 1), endpoint=False)
        filters[i, center:right] = np.linspace(1.0, 0.0, max(right - center, 1), endpoint=False)

    return filters


def compute_log_mel(audio: np.ndarray,
                    sample_rate: int = 16000,
                    n_mels: int = 64,
                    window_ms: float = 25.0,
                    hop_ms: float = (1000.0 / 60.0)) -> np.ndarray:
    n_fft = int(sample_rate * window_ms / 1000.0)
    hop_length = int(sample_rate * hop_ms / 1000.0)
    _, _, spec = stft(audio, fs=sample_rate, nperseg=n_fft, noverlap=n_fft - hop_length, padded=False, boundary=None)
    power = np.abs(spec) ** 2
    mel_filters = build_mel_filterbank(sample_rate, n_fft, n_mels)
    mel = mel_filters @ power
    return np.log10(np.maximum(mel, 1e-10)).astype(np.float32)
```

- [ ] **Step 4: Run the feature-pipeline test to verify it passes**

Run: `python -m unittest python_scripts.tests.test_audio_feature_pipeline -v`  
Expected: PASS with `OK`

- [ ] **Step 5: Run the full project verification set**

Run: `python python_scripts/tests/check_audio_config.py && python python_scripts/tests/check_stream_wiring.py && python -m unittest python_scripts.tests.test_audio_stream_protocol python_scripts.tests.test_audio_feature_pipeline -v && make -j4`  
Expected: PASS with all Python checks green and the firmware build succeeding

- [ ] **Step 6: Commit the host-side feature pipeline**

```bash
git add requirements.txt python_scripts/audio_feature_pipeline.py python_scripts/tests/test_audio_feature_pipeline.py
git commit -m "feat: add host-side 16khz log-mel feature pipeline"
```
