/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : my_main.h
  * @brief          : Header for my_main.c file.
  *                   This file contains the application sensor logic.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MY_MAIN_H__
#define __MY_MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "vcnl4040_sensor.h"
#include "icm42688_sensor.h"
#include "microphone_sensor.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern VCNL4040_HandleTypeDef vcnl4040;
extern ICM42688_HandleTypeDef icm42688;
extern MIC_HandleTypeDef mic;
extern MIC_HandleTypeDef mic_2;

/* Exported functions prototypes ---------------------------------------------*/
void My_Application_Init(void);
void My_Application_Run(void);  // Contains infinite loop, never returns

#ifdef __cplusplus
}
#endif

#endif /* __MY_MAIN_H__ */
