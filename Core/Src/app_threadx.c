/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @brief   ThreadX application file
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "wifi_8266.h"
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
static TX_THREAD blink_thread;
static UCHAR blink_thread_stack[WIFI8266_BLINK_THREAD_STACK_SIZE];

static TX_THREAD log_thread;
static UCHAR log_thread_stack[WIFI8266_LOG_THREAD_STACK_SIZE];

static TX_THREAD wifi8266_thread;
static UCHAR wifi8266_thread_stack[WIFI8266_THREAD_STACK_SIZE];

volatile UINT threadx_boot_stage;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

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

  threadx_boot_stage = 1U;

  ret = tx_thread_create(&blink_thread,
                         "blink_thread",
                         wifi8266_blink_thread_entry,
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

  ret = tx_thread_create(&log_thread,
                         "log_thread",
                         wifi8266_log_thread_entry,
                         0,
                         log_thread_stack,
                         sizeof(log_thread_stack),
                         16,
                         16,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    Error_Handler();
  }

  ret = tx_thread_create(&wifi8266_thread,
                         "wifi8266_thread",
                         wifi8266_service_thread_entry,
                         0,
                         wifi8266_thread_stack,
                         sizeof(wifi8266_thread_stack),
                         17,
                         17,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    Error_Handler();
  }

  threadx_boot_stage = 2U;
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
  threadx_boot_stage = 10U;
  HAL_GPIO_WritePin(KEEP_GPIO_Port, KEEP_Pin, GPIO_PIN_SET);
  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
