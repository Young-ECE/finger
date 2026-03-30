/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : my_main.c
  * @brief          : Application sensor logic implementation
  ******************************************************************************
  * @attention
  *
  * This file contains all user-defined sensor initialization and data
  * acquisition logic, separated from CubeMX-generated main.c framework.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "my_application.h"

#include "audio_stream_transport.h"
#include "i2s.h"
#include "methods.h"
#include "usbd_cdc_if.h"

/* Private variables ---------------------------------------------------------*/
extern I2S_HandleTypeDef hi2s1;
extern I2S_HandleTypeDef hi2s2;
extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint32_t dma_buffer[MIC_BUFFER_SIZE];
extern uint32_t dma_buffer_2[MIC_BUFFER_SIZE];

ICM42688_HandleTypeDef icm42688;
MIC_HandleTypeDef mic;
MIC_HandleTypeDef mic_2;

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
static int16_t audio_pcm_block[AUDIO_STREAM_PAYLOAD_SAMPLES];
static uint8_t audio_tx_head;
static uint8_t audio_tx_tail;
static uint8_t audio_tx_count;
static uint8_t audio_tx_in_flight;
static uint32_t audio_sequence;
static uint8_t audio_status_flags;

static uint8_t AudioTx_NextIndex(uint8_t index)
{
  return (uint8_t)((index + 1U) % AUDIO_TX_QUEUE_DEPTH);
}

static uint8_t AudioTx_IsQueueEmpty(void)
{
  return (audio_tx_count == 0U);
}

static uint8_t AudioTx_IsQueueFull(void)
{
  return (audio_tx_count >= AUDIO_TX_QUEUE_DEPTH);
}

static uint8_t AudioTx_IsUsbIdle(void)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

  return (uint8_t)((hcdc != NULL) && (hcdc->TxState == 0U));
}

static void AudioTx_Reset(void)
{
  audio_tx_head = 0U;
  audio_tx_tail = 0U;
  audio_tx_count = 0U;
  audio_tx_in_flight = 0U;
  audio_sequence = 0U;
  audio_status_flags = 0U;
}

static void AudioTx_QueueBlock(const uint32_t *left_block,
                               const uint32_t *right_block,
                               uint8_t flags)
{
  uint32_t sequence = audio_sequence++;
  AudioTxPacket *packet;
  uint8_t packet_flags;

  if (AudioTx_IsQueueFull() != 0U)
  {
    audio_status_flags |= AUDIO_FLAG_QUEUE_OVERFLOW;
    return;
  }

  packet_flags = (uint8_t)(flags | audio_status_flags);
  packet = &audio_tx_queue[audio_tx_tail];

  AudioStream_InterleavePcmBlock(left_block, right_block, audio_pcm_block);
  packet->length = AudioStream_BuildPacket(packet->bytes, sequence, packet_flags, audio_pcm_block);

  audio_status_flags = 0U;
  audio_tx_tail = AudioTx_NextIndex(audio_tx_tail);
  audio_tx_count++;
}

static void AudioTx_Process(void)
{
  AudioTxPacket *packet;

  if (audio_tx_in_flight != 0U)
  {
    if (AudioTx_IsUsbIdle() == 0U)
    {
      return;
    }

    audio_tx_head = AudioTx_NextIndex(audio_tx_head);
    audio_tx_count--;
    audio_tx_in_flight = 0U;
  }

  if (AudioTx_IsQueueEmpty() != 0U)
  {
    return;
  }

  packet = &audio_tx_queue[audio_tx_head];
  if (CDC_Transmit_FS(packet->bytes, packet->length) == USBD_OK)
  {
    audio_tx_in_flight = 1U;
  }
}

/* Public functions ----------------------------------------------------------*/

/**
  * @brief  Initialize all sensors and peripherals
  * @retval None
  */
void My_Application_Init(void)
{
  AudioTx_Reset();

  RGB_LED_Init();
  HAL_Delay(100U);

  MIC_Init(&mic, &hi2s1);
  MIC_Init(&mic_2, &hi2s2);
  MIC_Start(&mic, (uint16_t *)dma_buffer);
  MIC_Start(&mic_2, (uint16_t *)dma_buffer_2);
  HAL_Delay(100U);
}

/**
  * @brief  Main sensor reading loop (infinite)
  * @retval None (never returns)
  */
void My_Application_Run(void)
{
  while (1)
  {
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
