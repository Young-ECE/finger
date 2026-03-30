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

    return ((int32_t)packed) >> 8;
}

int16_t AudioStream_Raw24ToPcm16(int32_t raw24)
{
    return (int16_t)(raw24 >> 8);
}

void AudioStream_InterleavePcmBlock(const uint32_t *left_buffer,
                                    const uint32_t *right_buffer,
                                    int16_t *dst_interleaved)
{
    uint16_t i;

    for (i = 0U; i < AUDIO_STREAM_SAMPLES_PER_PACKET; ++i)
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
    AudioStream_WriteLe16(&dst[14], (uint16_t)AUDIO_STREAM_PAYLOAD_BYTES);
    memcpy(&dst[AUDIO_STREAM_HEADER_BYTES], interleaved_pcm, AUDIO_STREAM_PAYLOAD_BYTES);

    return AUDIO_STREAM_PACKET_BYTES;
}
