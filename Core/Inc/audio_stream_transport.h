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
