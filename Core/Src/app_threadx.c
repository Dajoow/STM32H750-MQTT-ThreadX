/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_azure_rtos_config.h"
#include "esp8266.h"
#include "main.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
#define BLINK_THREAD_STACK_SIZE 1024

static TX_THREAD blink_thread;
static UCHAR blink_thread_stack[BLINK_THREAD_STACK_SIZE];

volatile UINT blink_thread_count;
volatile UINT wifi_service_status;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID blink_thread_entry(ULONG thread_input);
static UINT wifi_service_init(TX_BYTE_POOL *pool);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */
(void)memory_ptr;

    ret = tx_thread_create(&blink_thread,
                         "blink_thread",
                         blink_thread_entry,
                         0,
                         blink_thread_stack,
                         sizeof(blink_thread_stack),
                         15,
                         15,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    Error_Handler();
  }
  wifi_service_status = wifi_service_init(TX_NULL);
  if (wifi_service_status != TX_SUCCESS)
  {
    Error_Handler();
  }
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */
HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_SET);
  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 2 */
static VOID blink_thread_entry(ULONG thread_input)
{
  (void)thread_input;
  for (;;)
  {
    blink_thread_count++;
    HAL_GPIO_TogglePin(RELAY_GPIO_Port, RELAY_Pin);
    tx_thread_sleep(1000);
  }
}


static UINT wifi_service_init(TX_BYTE_POOL *pool)
{
  UINT status;

  status = esp8266_service_init(pool);
  if (status != TX_SUCCESS)
  {
    printf("[E][WIFI] service init failed: %u\r\n", (unsigned int)status);
  }

  return status;
}
/* USER CODE END 2 */
