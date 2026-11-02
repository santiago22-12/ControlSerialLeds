/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Cabecera de main.c - ControlSerialLeds
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* USER CODE BEGIN EM */

/* USER CODE END EM */

void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PULSADOR_Pin GPIO_PIN_0
#define PULSADOR_GPIO_Port GPIOA
#define PULSADOR_EXTI_IRQn EXTI0_IRQn
#define LATIDO_Pin GPIO_PIN_0
#define LATIDO_GPIO_Port GPIOB
#define TIMER_RAPIDO_Pin GPIO_PIN_1
#define TIMER_RAPIDO_GPIO_Port GPIOB
#define TIMER_LENTO_Pin GPIO_PIN_10
#define TIMER_LENTO_GPIO_Port GPIOB
#define LUZ_PULSADOR_Pin GPIO_PIN_5
#define LUZ_PULSADOR_GPIO_Port GPIOB
#define LUZ_SERIAL_Pin GPIO_PIN_8
#define LUZ_SERIAL_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
