/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  MCD Application Team
  * @brief   NetXDuo applicative file
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
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "nxd_dhcp_client.h"
#include "nxd_mqtt_client.h"
#include "main.h"
#include "math.h"
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MQTT_APP_THREAD_STACK_SIZE 4096
#define MQTT_CLIENT_STACK_SIZE     4096
#define MQTT_CLIENT_MEMORY_SIZE    2048

#define MQTT_APP_THREAD_PRIORITY   12
#define MQTT_THREAD_PRIORITY       13

#define MQTT_USE_DHCP              0
#define MQTT_STATIC_GATEWAY        IP_ADDRESS(192, 168, 50, 1)

#define MQTT_BROKER_ADDRESS        IP_ADDRESS(192, 168, 50, 1)
#define MQTT_BROKER_PORT           NXD_MQTT_PORT
#define MQTT_KEEPALIVE_SECONDS     60
#define MQTT_PUBLISH_PERIOD_TICKS  (5 * NX_IP_PERIODIC_RATE)
#define MQTT_COMMAND_POLL_TICKS    (((NX_IP_PERIODIC_RATE / 10) > 0) ? (NX_IP_PERIODIC_RATE / 10) : 1)

#define MQTT_CLIENT_ID             "stm32h750-threadx"
#define MQTT_USERNAME              ""
#define MQTT_PASSWORD              ""
#define MQTT_PUBLISH_TOPIC         "stm32/h750/status"
#define MQTT_LED_SET_TOPIC         "stm32/h750/led/set"
#define MQTT_LED_STATE_TOPIC       "stm32/h750/led/state"
#define MQTT_ONLINE_MESSAGE        "online"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#ifndef NX_APP_WEAK
#if defined(__GNUC__) || defined(__clang__)
#define NX_APP_WEAK __attribute__((weak))
#elif defined(__CC_ARM)
#define NX_APP_WEAK __weak
#else
#define NX_APP_WEAK
#endif
#endif
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
/* USER CODE BEGIN PV */
static NX_DHCP nx_app_dhcp;
static NXD_MQTT_CLIENT mqtt_client;
static TX_THREAD mqtt_app_thread;

static UCHAR *mqtt_app_thread_stack;
static UCHAR *mqtt_client_stack;
static UCHAR *mqtt_client_memory;

volatile UINT nx_app_last_status;
volatile UINT nx_app_mqtt_status;
volatile ULONG nx_app_actual_status;
volatile ULONG nx_app_link_status;
volatile ULONG nx_app_ip_address;
volatile ULONG nx_app_network_mask;
volatile ULONG nx_app_publish_count;
volatile UINT nx_app_init_step;
volatile UINT nx_app_init_status;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID nx_app_thread_entry (ULONG thread_input);
/* USER CODE BEGIN PFP */
static VOID mqtt_app_thread_entry(ULONG thread_input);
static UINT nx_app_allocate(TX_BYTE_POOL *byte_pool, VOID **pointer, ULONG size);
static VOID mqtt_led_apply_command(UCHAR *message, UINT message_length);
static UINT mqtt_led_publish_state(ULONG wait_option);
VOID nx_stm32_eth_driver(NX_IP_DRIVER *driver_req_ptr);
/* USER CODE END PFP */

/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer;

  nx_app_init_step = 1;
  nx_app_init_status = NX_SUCCESS;

  /* USER CODE BEGIN MX_NetXDuo_MEM_POOL */
  ret = nx_app_allocate(byte_pool, (VOID **)&mqtt_app_thread_stack, MQTT_APP_THREAD_STACK_SIZE);
  nx_app_init_status = ret;
  if (ret != NX_SUCCESS)
  {
    return ret;
  }

  nx_app_init_step = 2;
  ret = nx_app_allocate(byte_pool, (VOID **)&mqtt_client_stack, MQTT_CLIENT_STACK_SIZE);
  nx_app_init_status = ret;
  if (ret != NX_SUCCESS)
  {
    return ret;
  }

  nx_app_init_step = 3;
  ret = nx_app_allocate(byte_pool, (VOID **)&mqtt_client_memory, MQTT_CLIENT_MEMORY_SIZE);
  nx_app_init_status = ret;
  if (ret != NX_SUCCESS)
  {
    return ret;
  }
  /* USER CODE END MX_NetXDuo_MEM_POOL */

  /* USER CODE BEGIN 0 */

  /* USER CODE END 0 */

  /* Initialize the NetXDuo system. */
  nx_app_init_step = 4;
  nx_system_initialize();

    /* Allocate the memory for packet_pool.  */
  nx_app_init_step = 5;
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    nx_app_init_status = TX_POOL_ERROR;
    return TX_POOL_ERROR;
  }

  /* Create the Packet pool to be used for packet allocation,
   * If extra NX_PACKET are to be used the NX_APP_PACKET_POOL_SIZE should be increased
   */
  ret = nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool", DEFAULT_PAYLOAD_SIZE, pointer, NX_APP_PACKET_POOL_SIZE);
  nx_app_init_status = ret;

  if (ret != NX_SUCCESS)
  {
    return NX_POOL_ERROR;
  }

    /* Allocate the memory for Ip_Instance */
  nx_app_init_step = 6;
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    nx_app_init_status = TX_POOL_ERROR;
    return TX_POOL_ERROR;
  }

   /* Create the main NX_IP instance */
  nx_app_init_step = 7;
  ret = nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance", NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK, &NxAppPool, nx_stm32_eth_driver,
                     pointer, Nx_IP_INSTANCE_THREAD_SIZE, NX_APP_INSTANCE_PRIORITY);
  nx_app_init_status = ret;

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

    /* Allocate the memory for ARP */
  nx_app_init_step = 8;
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    nx_app_init_status = TX_POOL_ERROR;
    return TX_POOL_ERROR;
  }

  /* Enable the ARP protocol and provide the ARP cache size for the IP instance */

  /* USER CODE BEGIN ARP_Protocol_Initialization */

  /* USER CODE END ARP_Protocol_Initialization */

  ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);
  nx_app_init_status = ret;

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the ICMP */

  /* USER CODE BEGIN ICMP_Protocol_Initialization */

  /* USER CODE END ICMP_Protocol_Initialization */

  nx_app_init_step = 9;
  ret = nx_icmp_enable(&NetXDuoEthIpInstance);
  nx_app_init_status = ret;

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable TCP Protocol */

  /* USER CODE BEGIN TCP_Protocol_Initialization */

  /* USER CODE END TCP_Protocol_Initialization */

  nx_app_init_step = 10;
  ret = nx_tcp_enable(&NetXDuoEthIpInstance);
  nx_app_init_status = ret;

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the UDP protocol required for  DHCP communication */

  /* USER CODE BEGIN UDP_Protocol_Initialization */

  /* USER CODE END UDP_Protocol_Initialization */

  nx_app_init_step = 11;
  ret = nx_udp_enable(&NetXDuoEthIpInstance);
  nx_app_init_status = ret;

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

   /* Allocate the memory for main thread   */
  nx_app_init_step = 12;
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    nx_app_init_status = TX_POOL_ERROR;
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  nx_app_init_step = 13;
  ret = tx_thread_create(&NxAppThread, "NetXDuo App thread", nx_app_thread_entry , 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
  nx_app_init_status = ret;

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE BEGIN MX_NetXDuo_Init */
  nx_app_init_step = 14;
#if MQTT_USE_DHCP
  ret = nx_dhcp_create(&nx_app_dhcp, &NetXDuoEthIpInstance, "NetX DHCP client");
  nx_app_init_status = ret;
  if (ret != NX_SUCCESS)
  {
    return ret;
  }
#else
  ret = nx_ip_gateway_address_set(&NetXDuoEthIpInstance, MQTT_STATIC_GATEWAY);
  nx_app_init_status = ret;
  if (ret != NX_SUCCESS)
  {
    return ret;
  }
#endif

  nx_app_init_step = 15;
  ret = tx_thread_create(&mqtt_app_thread,
                         "mqtt_app_thread",
                         mqtt_app_thread_entry,
                         0,
                         mqtt_app_thread_stack,
                         MQTT_APP_THREAD_STACK_SIZE,
                         MQTT_APP_THREAD_PRIORITY,
                         MQTT_APP_THREAD_PRIORITY,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);
  nx_app_init_status = ret;

  /* USER CODE END MX_NetXDuo_Init */

  nx_app_init_step = 16;
  return ret;
}

/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID nx_app_thread_entry (ULONG thread_input)
{
  /* USER CODE BEGIN Nx_App_Thread_Entry 0 */

  /* USER CODE END Nx_App_Thread_Entry 0 */

}
/* USER CODE BEGIN 2 */
static UINT nx_app_allocate(TX_BYTE_POOL *byte_pool, VOID **pointer, ULONG size)
{
  UINT status;

  status = tx_byte_allocate(byte_pool, pointer, size, TX_NO_WAIT);
  nx_app_last_status = status;

  return status;
}

static VOID mqtt_app_thread_entry(ULONG thread_input)
{
  UINT status;
  NXD_ADDRESS broker_address;
  ULONG actual_status;
  ULONG publish_elapsed_ticks = 0;
  CHAR message[32];
  UINT message_length;
  UCHAR topic_buffer[64];
  UCHAR payload_buffer[32];
  UINT actual_topic_length;
  UINT actual_message_length;

  (void)thread_input;

  status = nx_ip_status_check(&NetXDuoEthIpInstance,
                              NX_IP_INITIALIZE_DONE,
                              &actual_status,
                              10 * NX_IP_PERIODIC_RATE);
  nx_app_last_status = status;
  nx_app_actual_status = actual_status;
  if (status != NX_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }

  status = nx_ip_status_check(&NetXDuoEthIpInstance,
                              NX_IP_LINK_ENABLED | NX_IP_INTERFACE_LINK_ENABLED,
                              &actual_status,
                              10 * NX_IP_PERIODIC_RATE);
  nx_app_last_status = status;
  nx_app_link_status = actual_status;
  if (status != NX_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }

#if MQTT_USE_DHCP
  status = nx_dhcp_start(&nx_app_dhcp);
  nx_app_last_status = status;
  if (status != NX_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }

  status = nx_ip_status_check(&NetXDuoEthIpInstance,
                              NX_IP_ADDRESS_RESOLVED,
                              &actual_status,
                              30 * NX_IP_PERIODIC_RATE);
  nx_app_last_status = status;
  nx_app_actual_status = actual_status;
  if (status != NX_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }
#endif

  nx_ip_address_get(&NetXDuoEthIpInstance, (ULONG *)&nx_app_ip_address, (ULONG *)&nx_app_network_mask);

  status = nxd_mqtt_client_create(&mqtt_client,
                                  "STM32 MQTT client",
                                  MQTT_CLIENT_ID,
                                  (UINT)strlen(MQTT_CLIENT_ID),
                                  &NetXDuoEthIpInstance,
                                  &NxAppPool,
                                  mqtt_client_stack,
                                  MQTT_CLIENT_STACK_SIZE,
                                  MQTT_THREAD_PRIORITY,
                                  mqtt_client_memory,
                                  MQTT_CLIENT_MEMORY_SIZE);
  nx_app_mqtt_status = status;
  if (status != NXD_MQTT_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }
  if (strlen(MQTT_USERNAME) > 0)
  {
    status = nxd_mqtt_client_login_set(&mqtt_client,
                                       MQTT_USERNAME,
                                       (UINT)strlen(MQTT_USERNAME),
                                       MQTT_PASSWORD,
                                       (UINT)strlen(MQTT_PASSWORD));
    nx_app_mqtt_status = status;
    if (status != NXD_MQTT_SUCCESS)
    {
      tx_thread_suspend(tx_thread_identify());
    }
  }

  broker_address.nxd_ip_version = NX_IP_VERSION_V4;
  broker_address.nxd_ip_address.v4 = MQTT_BROKER_ADDRESS;

  status = nxd_mqtt_client_connect(&mqtt_client,
                                   &broker_address,
                                   MQTT_BROKER_PORT,
                                   MQTT_KEEPALIVE_SECONDS,
                                   NX_TRUE,
                                   10 * NX_IP_PERIODIC_RATE);
  nx_app_mqtt_status = status;
  if (status != NXD_MQTT_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }

  status = nxd_mqtt_client_publish(&mqtt_client,
                                   MQTT_PUBLISH_TOPIC,
                                   (UINT)strlen(MQTT_PUBLISH_TOPIC),
                                   MQTT_ONLINE_MESSAGE,
                                   (UINT)strlen(MQTT_ONLINE_MESSAGE),
                                   NX_FALSE,
                                   0,
                                   5 * NX_IP_PERIODIC_RATE);
  nx_app_mqtt_status = status;
  if (status == NXD_MQTT_SUCCESS)
  {
    nx_app_publish_count++;
  }

  status = nxd_mqtt_client_subscribe(&mqtt_client,
                                     MQTT_LED_SET_TOPIC,
                                     (UINT)strlen(MQTT_LED_SET_TOPIC),
                                     0);
  nx_app_mqtt_status = status;
  if (status != NXD_MQTT_SUCCESS)
  {
    tx_thread_suspend(tx_thread_identify());
  }

  (void)mqtt_led_publish_state(5 * NX_IP_PERIODIC_RATE);

  for (;;)
  {
    status = nxd_mqtt_client_message_get(&mqtt_client,
                                         topic_buffer,
                                         sizeof(topic_buffer),
                                         &actual_topic_length,
                                         payload_buffer,
                                         sizeof(payload_buffer),
                                         &actual_message_length);
    if (status == NXD_MQTT_SUCCESS)
    {
      if ((actual_topic_length == strlen(MQTT_LED_SET_TOPIC)) &&
          (memcmp(topic_buffer, MQTT_LED_SET_TOPIC, actual_topic_length) == 0))
      {
        mqtt_led_apply_command(payload_buffer, actual_message_length);
      }
      continue;
    }

    tx_thread_sleep(MQTT_COMMAND_POLL_TICKS);
    publish_elapsed_ticks += MQTT_COMMAND_POLL_TICKS;
    if (publish_elapsed_ticks < MQTT_PUBLISH_PERIOD_TICKS)
    {
      continue;
    }
    publish_elapsed_ticks = 0;

    message_length = (UINT)snprintf(message, sizeof(message), "tick=%lu", nx_app_publish_count);
    if (message_length >= sizeof(message))
    {
      message_length = sizeof(message) - 1;
    }

    status = nxd_mqtt_client_publish(&mqtt_client,
                                     MQTT_PUBLISH_TOPIC,
                                     (UINT)strlen(MQTT_PUBLISH_TOPIC),
                                     message,
                                     message_length,
                                     NX_FALSE,
                                     0,
                                     5 * NX_IP_PERIODIC_RATE);
    nx_app_mqtt_status = status;
    if (status == NXD_MQTT_SUCCESS)
    {
      nx_app_publish_count++;
    }
  }
}

static VOID mqtt_led_apply_command(UCHAR *message, UINT message_length)
{
  if ((message_length == 2) && (memcmp(message, "ON", 2) == 0))
  {
    HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_RESET);
  }
  else if ((message_length == 3) && (memcmp(message, "OFF", 3) == 0))
  {
    HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_SET);
  }
  else if ((message_length == 6) && (memcmp(message, "TOGGLE", 6) == 0))
  {
    HAL_GPIO_TogglePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin);
  }
  else
  {
    return;
  }

  (void)mqtt_led_publish_state(5 * NX_IP_PERIODIC_RATE);
}

static UINT mqtt_led_publish_state(ULONG wait_option)
{
  CHAR *state;
  UINT status;
  UINT led_state;

  led_state = (HAL_GPIO_ReadPin(LED_CTRL_GPIO_Port, LED_CTRL_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
  state = led_state ? "ON" : "OFF";

  status = nxd_mqtt_client_publish(&mqtt_client,
                                   MQTT_LED_STATE_TOPIC,
                                   (UINT)strlen(MQTT_LED_STATE_TOPIC),
                                   state,
                                   (UINT)strlen(state),
                                   NX_FALSE,
                                   0,
                                   wait_option);

  return status;
}

NX_APP_WEAK VOID nx_stm32_eth_driver(NX_IP_DRIVER *driver_req_ptr)
{
  driver_req_ptr -> nx_ip_driver_status = NX_UNHANDLED_COMMAND;
}

/* USER CODE END 2 */
