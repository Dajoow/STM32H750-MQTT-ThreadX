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
#include "app_netxduo.h"
#include "main.h"
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
#define NETX_INIT_THREAD_STACK_SIZE 4096
#define NETX_INIT_ENABLE 1

static TX_THREAD blink_thread;
static UCHAR blink_thread_stack[BLINK_THREAD_STACK_SIZE];
static TX_THREAD netx_init_thread;
static UCHAR netx_init_thread_stack[NETX_INIT_THREAD_STACK_SIZE];
static TX_BYTE_POOL netx_init_byte_pool;
static UCHAR netx_init_byte_pool_buffer[NX_APP_MEM_POOL_SIZE] __attribute__((section(".bss.RAM_D1"), aligned(32)));

volatile UINT netx_init_thread_step;
volatile UINT netx_init_thread_status;
volatile UINT blink_thread_count;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID blink_thread_entry(ULONG thread_input);
static VOID netx_init_thread_entry(ULONG thread_input);
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

  ret = tx_thread_create(&netx_init_thread,
                         "netx_init_thread",
                         netx_init_thread_entry,
                         0,
                         netx_init_thread_stack,
                         sizeof(netx_init_thread_stack),
                         16,
                         16,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);

  if (ret != TX_SUCCESS)
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

static VOID netx_init_thread_entry(ULONG thread_input)
{
  (void)thread_input;

  netx_init_thread_step = 1;
  tx_thread_sleep(2 * TX_TIMER_TICKS_PER_SECOND);

#if (NETX_INIT_ENABLE == 0)
  netx_init_thread_step = 100;
  tx_thread_suspend(tx_thread_identify());
#endif

  netx_init_thread_step = 2;
  netx_init_thread_status = tx_byte_pool_create(&netx_init_byte_pool,
                                                "NetX deferred pool",
                                                netx_init_byte_pool_buffer,
                                                sizeof(netx_init_byte_pool_buffer));
  if (netx_init_thread_status != TX_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }

  netx_init_thread_step = 3;
  netx_init_thread_status = MX_NetXDuo_Init(&netx_init_byte_pool);

  netx_init_thread_step = 4;
  tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);

  netx_init_thread_step = 5;
  tx_thread_suspend(tx_thread_identify());
}
/* USER CODE END 2 */
