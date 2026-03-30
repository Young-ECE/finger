#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio_stream_transport.h"

static void test_extract_and_convert(void)
{
    uint32_t words[AUDIO_STREAM_DMA_WORDS_PER_BLOCK] = {0};

    words[0] = 0x0001U;
    words[1] = 0x0000U;
    words[2] = 0xFFFFU;
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
