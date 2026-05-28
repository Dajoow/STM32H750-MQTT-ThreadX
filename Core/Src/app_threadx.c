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
#include "usart.h"
#include <string.h>
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
#define BLINK_THREAD_STACK_SIZE 1024U
#define LOG_THREAD_STACK_SIZE   1024U
#define ESP8266_TEST_THREAD_STACK_SIZE 2048U
#define ESP8266_RX_RING_SIZE 256U
#define ESP8266_RESP_BUF_SIZE 512U

#define ESP8266_WIFI_SSID "iQOO Z10 Turbo+"
#define ESP8266_WIFI_PASS "djwdjwdjw"
#define ESP8266_MQTT_HOST "broker.emqx.io"
#define ESP8266_MQTT_PORT "1883"
#define ESP8266_MQTT_CLIENT_ID "stm32_client_001"
#define ESP8266_MQTT_TOPIC "test/topic"

static TX_THREAD blink_thread;
static UCHAR blink_thread_stack[BLINK_THREAD_STACK_SIZE];
static TX_THREAD log_thread;
static UCHAR log_thread_stack[LOG_THREAD_STACK_SIZE];
static TX_THREAD esp8266_test_thread;
static UCHAR esp8266_test_thread_stack[ESP8266_TEST_THREAD_STACK_SIZE];

static uint8_t esp8266_rx_byte;
static uint8_t esp8266_rx_ring[ESP8266_RX_RING_SIZE];
static volatile uint16_t esp8266_rx_head;
static volatile uint16_t esp8266_rx_tail;
static volatile UINT esp8266_rx_overflow;

volatile UINT blink_thread_count;
volatile UINT log_thread_count;
volatile UINT esp8266_test_thread_count;
volatile UINT threadx_boot_stage;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID blink_thread_entry(ULONG thread_input);
static VOID log_thread_entry(ULONG thread_input);
static VOID esp8266_test_thread_entry(ULONG thread_input);
static void debug_uart3_write(const char *text);
static UINT esp8266_send_cmd(const char *cmd);
static UINT esp8266_wait_for(const char *ok_token, const char *fail_token, ULONG timeout_ticks);
static UINT esp8266_rx_pop(uint8_t *byte);
static void esp8266_rx_flush(void);
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

  ret = tx_thread_create(&log_thread,
                         "log_thread",
                         log_thread_entry,
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

  ret = tx_thread_create(&esp8266_test_thread,
                         "esp8266_test_thread",
                         esp8266_test_thread_entry,
                         0,
                         esp8266_test_thread_stack,
                         sizeof(esp8266_test_thread_stack),
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
//    printf("S\r\n");
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
    HAL_GPIO_TogglePin(KEEP_GPIO_Port, KEEP_Pin);
    tx_thread_sleep(500U);
  }
}

static VOID log_thread_entry(ULONG thread_input)
{
  (void)thread_input;

  tx_thread_sleep(1000U);

  for (;;)
  {
    log_thread_count++;
    debug_uart3_write("[LOG] USART3 raw transmit alive\r\n");
    tx_thread_sleep(2000U);
  }
}

static VOID esp8266_test_thread_entry(ULONG thread_input)
{
  (void)thread_input;

  tx_thread_sleep(1500U);
  debug_uart3_write("[ESP8266] WiFi MQTT test start\r\n");

  (void)HAL_UART_Receive_IT(&huart1, &esp8266_rx_byte, 1U);

  for (;;)
  {
    esp8266_test_thread_count++;

    debug_uart3_write("\r\n[ESP8266] STEP 1: AT\r\n");
    if ((esp8266_send_cmd("AT\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("OK", "ERROR", 200U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] AT failed, retry later\r\n");
      tx_thread_sleep(3000U);
      continue;
    }

    debug_uart3_write("\r\n[ESP8266] STEP 2: ATE0\r\n");
    if ((esp8266_send_cmd("ATE0\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("OK", "ERROR", 200U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] ATE0 failed, retry later\r\n");
      tx_thread_sleep(3000U);
      continue;
    }

    debug_uart3_write("\r\n[ESP8266] STEP 3: CWMODE STA\r\n");
    if ((esp8266_send_cmd("AT+CWMODE=1\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("OK", "ERROR", 300U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] CWMODE failed, retry later\r\n");
      tx_thread_sleep(3000U);
      continue;
    }

    debug_uart3_write("\r\n[ESP8266] STEP 4: JOIN WIFI\r\n");
    if ((esp8266_send_cmd("AT+CWJAP=\"" ESP8266_WIFI_SSID "\",\"" ESP8266_WIFI_PASS "\"\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("OK", "ERROR", 20000U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] WIFI connect failed, retry later\r\n");
      tx_thread_sleep(5000U);
      continue;
    }
    debug_uart3_write("[ESP8266] WIFI CONNECTED\r\n");

    debug_uart3_write("\r\n[ESP8266] STEP 5: MQTTUSERCFG\r\n");
    if ((esp8266_send_cmd("AT+MQTTUSERCFG=0,1,\"" ESP8266_MQTT_CLIENT_ID "\",\"\",\"\",0,0,\"\"\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("OK", "ERROR", 500U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] MQTTUSERCFG failed, retry later\r\n");
      tx_thread_sleep(3000U);
      continue;
    }

    debug_uart3_write("\r\n[ESP8266] STEP 6: MQTTCONN\r\n");
    if ((esp8266_send_cmd("AT+MQTTCONN=0,\"" ESP8266_MQTT_HOST "\"," ESP8266_MQTT_PORT ",1\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("+MQTTCONNECTED", "ERROR", 10000U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] MQTT connect failed, retry later\r\n");
      tx_thread_sleep(5000U);
      continue;
    }
    (void)esp8266_wait_for("OK", "ERROR", 300U);
    debug_uart3_write("[ESP8266] MQTT CONNECTED\r\n");

    debug_uart3_write("\r\n[ESP8266] STEP 7: MQTTSUB\r\n");
    if ((esp8266_send_cmd("AT+MQTTSUB=0,\"" ESP8266_MQTT_TOPIC "\",0\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("OK", "ERROR", 3000U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] MQTT subscribe failed, retry later\r\n");
      tx_thread_sleep(3000U);
      continue;
    }
    debug_uart3_write("[ESP8266] MQTT SUB OK\r\n");

    debug_uart3_write("\r\n[ESP8266] STEP 8: MQTTPUB\r\n");
    if ((esp8266_send_cmd("AT+MQTTPUB=0,\"" ESP8266_MQTT_TOPIC "\",\"hello from stm32\",0,0\r\n") != TX_SUCCESS) ||
        (esp8266_wait_for("OK", "ERROR", 3000U) != TX_SUCCESS))
    {
      debug_uart3_write("[ESP8266] MQTT publish failed, retry later\r\n");
      tx_thread_sleep(3000U);
      continue;
    }
    debug_uart3_write("[ESP8266] MQTT PUB OK\r\n");

    debug_uart3_write("[ESP8266] TEST DONE, monitor incoming data\r\n");

    for (;;)
    {
      (void)esp8266_wait_for("+MQTTSUBRECV", "WIFI DISCONNECT", 1000U);
      tx_thread_sleep(1000U);
    }
  }
}

static void debug_uart3_write(const char *text)
{
  if (text == TX_NULL)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart3,
                          (uint8_t *)text,
                          (uint16_t)strlen(text),
                          100U);
}

static UINT esp8266_send_cmd(const char *cmd)
{
  if (cmd == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  esp8266_rx_flush();
  debug_uart3_write("[ESP8266] TX: ");
  debug_uart3_write(cmd);

  if (HAL_UART_Transmit(&huart1,
                        (uint8_t *)cmd,
                        (uint16_t)strlen(cmd),
                        200U) != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
}

static UINT esp8266_wait_for(const char *ok_token, const char *fail_token, ULONG timeout_ticks)
{
  char response[ESP8266_RESP_BUF_SIZE];
  ULONG start_tick;
  uint16_t response_len = 0U;
  uint8_t byte;

  response[0] = '\0';
  start_tick = tx_time_get();

  while ((tx_time_get() - start_tick) < timeout_ticks)
  {
    while (esp8266_rx_pop(&byte) == TX_SUCCESS)
    {
      (void)HAL_UART_Transmit(&huart3, &byte, 1U, 20U);

      if (response_len < (ESP8266_RESP_BUF_SIZE - 1U))
      {
        response[response_len++] = (char)byte;
        response[response_len] = '\0';
      }

      if ((ok_token != TX_NULL) && (strstr(response, ok_token) != TX_NULL))
      {
        return TX_SUCCESS;
      }

      if ((fail_token != TX_NULL) && (strstr(response, fail_token) != TX_NULL))
      {
        return TX_NOT_DONE;
      }

      if ((strstr(response, "FAIL") != TX_NULL) ||
          (strstr(response, "busy p") != TX_NULL))
      {
        return TX_NOT_DONE;
      }
    }

    tx_thread_sleep(5U);
  }

  debug_uart3_write("\r\n[ESP8266] wait timeout\r\n");
  return TX_NOT_DONE;
}

static UINT esp8266_rx_pop(uint8_t *byte)
{
  if (esp8266_rx_tail == esp8266_rx_head)
  {
    return TX_NO_INSTANCE;
  }

  *byte = esp8266_rx_ring[esp8266_rx_tail];
  esp8266_rx_tail = (uint16_t)((esp8266_rx_tail + 1U) % ESP8266_RX_RING_SIZE);

  return TX_SUCCESS;
}

static void esp8266_rx_flush(void)
{
  uint8_t byte;

  while (esp8266_rx_pop(&byte) == TX_SUCCESS)
  {
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t next_head;

  if (huart->Instance == USART1)
  {
    next_head = (uint16_t)((esp8266_rx_head + 1U) % ESP8266_RX_RING_SIZE);

    if (next_head != esp8266_rx_tail)
    {
      esp8266_rx_ring[esp8266_rx_head] = esp8266_rx_byte;
      esp8266_rx_head = next_head;
    }
    else
    {
      esp8266_rx_overflow++;
    }

    (void)HAL_UART_Receive_IT(&huart1, &esp8266_rx_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    (void)HAL_UART_Receive_IT(&huart1, &esp8266_rx_byte, 1U);
  }
}

/* USER CODE END 2 */
