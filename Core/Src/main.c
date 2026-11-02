/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ControlSerialLeds
  *
  * Funciones:
  *   - LATIDO        (PB0) : parpadea cada 300 ms en el lazo principal (sin HAL_Delay)
  *   - TIMER_RAPIDO  (PB1) : cambia cada 250 ms por interrupción de TIM3
  *   - TIMER_LENTO   (PB10): cambia cada 2 s (cada 8 interrupciones de TIM3)
  *   - PULSADOR      (PA0) : interrupción EXTI por flanco de bajada (pull-up),
  *                           con antirrebote por software; cambia LUZ_PULSADOR (PB5)
  *   - USART2 (PA2 TX / PA3 RX, 115200 8N1): recepción por DMA + IDLE con la
  *     librería UARTRX. Comandos (no importan mayúsculas/minúsculas):
  *         ENCENDER | APAGAR | CAMBIAR | ESTADO | AYUDA
  *     Controlan LUZ_SERIAL (PB8).
  *
  * Reloj: HSI 16 MHz (sin PLL).
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "uart.h"
#include "UARTRX.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef void (*AccionComando)(void);

typedef struct
{
  const char   *texto;    /* palabra que se recibe por el puerto serial */
  AccionComando accion;   /* función que se ejecuta */
} Comando;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PERIODO_LATIDO_MS      300U
#define TICKS_TIMER_LENTO      8U     /* 8 x 250 ms = 2 s */
#define ANTIRREBOTE_MS         150U
#define TAM_COMANDO            32U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */
static uint32_t marcaLatido = 0;
volatile uint8_t  ticksTimer = 0;
volatile uint32_t ultimaPulsacion = 0;
volatile uint32_t totalPulsaciones = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void ActualizarLatido(void);
static void AtenderSerial(void);
static void LimpiarComando(const char *origen, char *destino, size_t tam);
static void CmdEncender(void);
static void CmdApagar(void);
static void CmdCambiar(void);
static void CmdEstado(void);
static void CmdAyuda(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Tabla de comandos: para agregar uno nuevo basta con una línea más */
static const Comando tablaComandos[] =
{
  { "ENCENDER", CmdEncender },
  { "APAGAR",   CmdApagar   },
  { "CAMBIAR",  CmdCambiar  },
  { "ESTADO",   CmdEstado   },
  { "AYUDA",    CmdAyuda    },
};
#define NUM_COMANDOS (sizeof(tablaComandos) / sizeof(tablaComandos[0]))

static void CmdEncender(void)
{
  HAL_GPIO_WritePin(LUZ_SERIAL_GPIO_Port, LUZ_SERIAL_Pin, GPIO_PIN_SET);
  uartx_write_text(&huart2, "OK: luz serial encendida\r\n");
}

static void CmdApagar(void)
{
  HAL_GPIO_WritePin(LUZ_SERIAL_GPIO_Port, LUZ_SERIAL_Pin, GPIO_PIN_RESET);
  uartx_write_text(&huart2, "OK: luz serial apagada\r\n");
}

static void CmdCambiar(void)
{
  HAL_GPIO_TogglePin(LUZ_SERIAL_GPIO_Port, LUZ_SERIAL_Pin);
  uartx_write_text(&huart2, "OK: luz serial invertida\r\n");
}

static void CmdEstado(void)
{
  char respuesta[64];
  GPIO_PinState estado = HAL_GPIO_ReadPin(LUZ_SERIAL_GPIO_Port, LUZ_SERIAL_Pin);

  snprintf(respuesta, sizeof(respuesta),
           "Luz serial: %s | pulsaciones: %lu\r\n",
           (estado == GPIO_PIN_SET) ? "ENCENDIDA" : "APAGADA",
           (unsigned long)totalPulsaciones);
  uartx_write_text(&huart2, respuesta);
}

static void CmdAyuda(void)
{
  uartx_write_text(&huart2, "Comandos: ENCENDER, APAGAR, CAMBIAR, ESTADO, AYUDA\r\n");
}

/**
  * @brief Copia el texto recibido quitando espacios/saltos de línea de los
  *        extremos y pasándolo a mayúsculas.
  */
static void LimpiarComando(const char *origen, char *destino, size_t tam)
{
  size_t i = 0;

  while ((*origen != '\0') && isspace((unsigned char)*origen))
  {
    origen++;
  }
  while ((*origen != '\0') && (i < (tam - 1U)))
  {
    destino[i++] = (char)toupper((unsigned char)*origen++);
  }
  destino[i] = '\0';

  while ((i > 0U) && isspace((unsigned char)destino[i - 1U]))
  {
    destino[--i] = '\0';
  }
}

/**
  * @brief Busca el comando recibido en la tabla y ejecuta su acción.
  */
static void AtenderSerial(void)
{
  char comando[TAM_COMANDO];
  uint32_t i;

  LimpiarComando((const char *)UARTRX1.trama_rx, comando, sizeof(comando));

  for (i = 0; i < NUM_COMANDOS; i++)
  {
    if (strcmp(comando, tablaComandos[i].texto) == 0)
    {
      tablaComandos[i].accion();
      return;
    }
  }
  uartx_write_text(&huart2, "Comando no reconocido. Escriba AYUDA\r\n");
}

static void ActualizarLatido(void)
{
  uint32_t ahora = HAL_GetTick();

  if ((ahora - marcaLatido) >= PERIODO_LATIDO_MS)
  {
    marcaLatido = ahora;
    HAL_GPIO_TogglePin(LATIDO_GPIO_Port, LATIDO_Pin);
  }
}

/**
  * @brief Interrupción de TIM3 (cada 250 ms).
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3)
  {
    HAL_GPIO_TogglePin(TIMER_RAPIDO_GPIO_Port, TIMER_RAPIDO_Pin);

    ticksTimer++;
    if (ticksTimer >= TICKS_TIMER_LENTO)
    {
      ticksTimer = 0;
      HAL_GPIO_TogglePin(TIMER_LENTO_GPIO_Port, TIMER_LENTO_Pin);
    }
  }
}

/**
  * @brief Interrupción externa del pulsador, con antirrebote por tiempo.
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == PULSADOR_Pin)
  {
    uint32_t ahora = HAL_GetTick();

    if ((ahora - ultimaPulsacion) >= ANTIRREBOTE_MS)
    {
      ultimaPulsacion = ahora;
      totalPulsaciones++;
      HAL_GPIO_TogglePin(LUZ_PULSADOR_GPIO_Port, LUZ_PULSADOR_Pin);
    }
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  uartRX_it_idle_dma_init(&UARTRX1);
  marcaLatido = HAL_GetTick();

  uartx_write_text(&huart2, "\r\n== ControlSerialLeds listo ==\r\n");
  CmdAyuda();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    ActualizarLatido();

    if (UARTRX1.flag_rx == 1)
    {
      AtenderSerial();
      uartRX_DMA_Re_init(&UARTRX1);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3 Initialization Function
  *        16 MHz / 16000 = 1 kHz  ->  250 cuentas = 250 ms
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 16000-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 250-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LATIDO_Pin|TIMER_RAPIDO_Pin|TIMER_LENTO_Pin|LUZ_PULSADOR_Pin
                          |LUZ_SERIAL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PULSADOR_Pin */
  GPIO_InitStruct.Pin = PULSADOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PULSADOR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LATIDO_Pin TIMER_RAPIDO_Pin TIMER_LENTO_Pin LUZ_PULSADOR_Pin
                           LUZ_SERIAL_Pin */
  GPIO_InitStruct.Pin = LATIDO_Pin|TIMER_RAPIDO_Pin|TIMER_LENTO_Pin|LUZ_PULSADOR_Pin
                          |LUZ_SERIAL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
