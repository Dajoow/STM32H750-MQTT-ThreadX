#include "esp8266.h"

#include "esp8266_at.h"
#include "esp8266_config.h"
#include "esp8266_mqtt.h"
#include "esp8266_ringbuf.h"

#include "main.h"
#include "usart.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* =============================
 * Logging
 * ============================= */
#define LOG_INFO(tag, fmt, ...)   printf("[I][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)   printf("[W][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...)  printf("[E][%s] " fmt "\r\n", tag, ##__VA_ARGS__)

/* =============================
 * ThreadX objects
 * ============================= */
static TX_THREAD s_rx_thread;
static TX_THREAD s_mqtt_thread;
static TX_THREAD s_heartbeat_thread;
static TX_EVENT_FLAGS_GROUP s_drv_flags;
static TX_MUTEX s_tx_mutex;

static UCHAR s_rx_stack[ESP8266_RX_THREAD_STACK] __attribute__((section(".bss.RAM_D1"), aligned(32)));
static UCHAR s_mqtt_stack[ESP8266_MQTT_THREAD_STACK] __attribute__((section(".bss.RAM_D1"), aligned(32)));
static UCHAR s_heartbeat_stack[ESP8266_HEARTBEAT_THREAD_STACK] __attribute__((section(".bss.RAM_D1"), aligned(32)));

/* =============================
 * UART DMA / RingBuffer
 * ============================= */
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

static uint8_t s_dma_rx_buf[ESP8266_DMA_RX_BUF_SIZE] __attribute__((section(".bss.RAM_D2"), aligned(32)));
static uint8_t s_ring_mem[ESP8266_RINGBUF_SIZE] __attribute__((section(".bss.RAM_D2"), aligned(32)));
static esp8266_ringbuf_t s_ring;
static volatile uint16_t s_dma_last_pos;

typedef enum
{
  ESP_LINK_BOOT = 0,
  ESP_LINK_WIFI_JOINING,
  ESP_LINK_WIFI_OK,
  ESP_LINK_MQTT_CONNECTING,
  ESP_LINK_MQTT_OK,
  ESP_LINK_RECOVER
} esp_link_state_t;

static volatile esp_link_state_t s_link_state = ESP_LINK_BOOT;

/* =============================
 * Local prototypes
 * ============================= */
static void rx_thread_entry(ULONG arg);
static void mqtt_thread_entry(ULONG arg);
static void heartbeat_thread_entry(ULONG arg);

static void dcache_invalidate(const void *addr, uint32_t len);
static void dma_pull_into_ring(void);
static UINT uart_dma_start(void);
static UINT at_wait_ok(ULONG timeout);
static UINT wifi_basic_init(void);
static UINT wifi_join_ap(void);
static UINT mqtt_online_init(void);
static void process_one_urc(esp8266_urc_t *urc);

/* =============================
 * Helpers
 * ============================= */
static void dcache_invalidate(const void *addr, uint32_t len)
{
#if (__DCACHE_PRESENT == 1U)
  uintptr_t start = (uintptr_t)addr;
  uintptr_t end = start + (uintptr_t)len;
  uintptr_t aligned_start = start & ~((uintptr_t)31U);
  uint32_t aligned_len = (uint32_t)((end - aligned_start + 31U) & ~((uintptr_t)31U));
  SCB_InvalidateDCache_by_Addr((uint32_t *)aligned_start, (int32_t)aligned_len);
#else
  (void)addr;
  (void)len;
#endif
}

static void dma_pull_into_ring(void)
{
  uint16_t pos;
  uint16_t n;

  if (huart1.hdmarx == NULL)
  {
    return;
  }

  pos = (uint16_t)(ESP8266_DMA_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx));
  dcache_invalidate(s_dma_rx_buf, sizeof(s_dma_rx_buf));

  if (pos == s_dma_last_pos)
  {
    return;
  }

  if (pos > s_dma_last_pos)
  {
    n = (uint16_t)(pos - s_dma_last_pos);
    (void)esp8266_ringbuf_write(&s_ring, &s_dma_rx_buf[s_dma_last_pos], n);
  }
  else
  {
    n = (uint16_t)(ESP8266_DMA_RX_BUF_SIZE - s_dma_last_pos);
    (void)esp8266_ringbuf_write(&s_ring, &s_dma_rx_buf[s_dma_last_pos], n);
    if (pos > 0U)
    {
      (void)esp8266_ringbuf_write(&s_ring, &s_dma_rx_buf[0], pos);
    }
  }

  s_dma_last_pos = pos;
  (void)tx_event_flags_set(&s_drv_flags, ESP8266_DRV_EVT_RX_READY, TX_OR);
}

static UINT uart_dma_start(void)
{
  s_dma_last_pos = 0U;

  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_dma_rx_buf, sizeof(s_dma_rx_buf)) != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  if (huart1.hdmarx != NULL)
  {
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  }

  return TX_SUCCESS;
}

UINT esp8266_send_at_line(const char *line, ULONG timeout)
{
  char txbuf[320];
  UINT status;

  if (line == NULL)
  {
    return TX_PTR_ERROR;
  }

  status = tx_mutex_get(&s_tx_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  (void)snprintf(txbuf, sizeof(txbuf), "%s\r\n", line);

  if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)txbuf, (uint16_t)strlen(txbuf)) != HAL_OK)
  {
    (void)tx_mutex_put(&s_tx_mutex);
    return TX_NOT_DONE;
  }

  status = tx_event_flags_get(&s_drv_flags,
                              ESP8266_DRV_EVT_TX_DONE,
                              TX_OR_CLEAR,
                              &(ULONG){0},
                              timeout);

  (void)tx_mutex_put(&s_tx_mutex);
  return status;
}

static UINT at_wait_ok(ULONG timeout)
{
  esp8266_at_result_t r;
  UINT s;

  s = esp8266_at_wait_result(&r,
                             timeout,
                             ESP8266_AT_FLG_OK | ESP8266_AT_FLG_ERROR | ESP8266_AT_FLG_FAIL | ESP8266_AT_FLG_BUSY,
                             0);
  if (s != TX_SUCCESS)
  {
    return s;
  }

  if (r.ok)
  {
    return TX_SUCCESS;
  }

  return TX_NOT_DONE;
}

static UINT wifi_basic_init(void)
{
  UINT s;

  LOG_INFO("WIFI", "WIFI INIT");

  s = esp8266_send_at_line("AT", ESP8266_TX_TIMEOUT);
  if (s != TX_SUCCESS) return s;
  s = at_wait_ok(ESP8266_CMD_TIMEOUT);
  if (s != TX_SUCCESS) return s;

  s = esp8266_send_at_line("ATE0", ESP8266_TX_TIMEOUT);
  if (s != TX_SUCCESS) return s;
  s = at_wait_ok(ESP8266_CMD_TIMEOUT);
  if (s != TX_SUCCESS) return s;

  s = esp8266_send_at_line("AT+CWMODE=1", ESP8266_TX_TIMEOUT);
  if (s != TX_SUCCESS) return s;
  s = at_wait_ok(ESP8266_CMD_TIMEOUT);
  if (s != TX_SUCCESS) return s;

  (void)esp8266_send_at_line("AT+CWRECONNCFG=1,10", ESP8266_TX_TIMEOUT);
  (void)at_wait_ok(ESP8266_CMD_TIMEOUT);

  return TX_SUCCESS;
}

static UINT wifi_join_ap(void)
{
  char cmd[192];
  UINT s;
  esp8266_at_result_t r;

  (void)snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASSWORD);

  s = esp8266_send_at_line(cmd, ESP8266_TX_TIMEOUT);
  if (s != TX_SUCCESS)
  {
    return s;
  }

  s = esp8266_at_wait_result(&r,
                             ESP8266_WIFI_JOIN_TIMEOUT,
                             ESP8266_AT_FLG_WIFI_GOT_IP | ESP8266_AT_FLG_OK | ESP8266_AT_FLG_ERROR | ESP8266_AT_FLG_FAIL,
                             0);
  if (s != TX_SUCCESS)
  {
    return s;
  }

  if (!(r.wifi_got_ip || r.ok))
  {
    return TX_NOT_DONE;
  }

  LOG_INFO("WIFI", "WIFI CONNECTED");
  return TX_SUCCESS;
}

static UINT mqtt_online_init(void)
{
  UINT s;

  s = esp8266_mqtt_connect();
  if (s != TX_SUCCESS)
  {
    return s;
  }

  s = esp8266_mqtt_subscribe(MQTT_SUB_TOPIC, MQTT_QOS0);
  if (s != TX_SUCCESS)
  {
    return s;
  }

  LOG_INFO("MQTT", "MQTT CONNECTED");

  s = esp8266_mqtt_publish(MQTT_PUB_TOPIC, "boot-online", MQTT_QOS0);
  if (s == TX_SUCCESS)
  {
    LOG_INFO("MQTT", "MQTT PUB OK");
  }

  return TX_SUCCESS;
}

static void process_one_urc(esp8266_urc_t *urc)
{
  if (urc == NULL)
  {
    return;
  }

  switch (urc->type)
  {
    case ESP8266_URC_WIFI_DISCONNECT:
      LOG_WARN("WIFI", "WIFI DISCONNECT");
      s_link_state = ESP_LINK_RECOVER;
      (void)tx_event_flags_set(&s_drv_flags, ESP8266_DRV_EVT_RECONNECT, TX_OR);
      break;

    case ESP8266_URC_MQTT_DISCONNECTED:
    case ESP8266_URC_CLOSED:
      LOG_WARN("MQTT", "MQTT CLOSED");
      s_link_state = ESP_LINK_RECOVER;
      (void)tx_event_flags_set(&s_drv_flags, ESP8266_DRV_EVT_RECONNECT, TX_OR);
      break;

    case ESP8266_URC_MQTT_SUBRECV:
      LOG_INFO("MQTT", "RX topic=%s payload=%s", urc->topic, urc->payload);
      esp8266_mqtt_notify_rx_topic(urc->topic, urc->payload, urc->payload_len);
      break;

    case ESP8266_URC_ERROR:
      LOG_ERROR("AT", "AT ERROR line=%s", urc->line);
      break;

    default:
      break;
  }
}

static void rx_thread_entry(ULONG arg)
{
  ULONG actual;
  uint8_t tmp[256];
  uint32_t n;
  esp8266_urc_t *urc;
  (void)arg;

  for (;;)
  {
    if (tx_event_flags_get(&s_drv_flags,
                           ESP8266_DRV_EVT_RX_READY,
                           TX_OR_CLEAR,
                           &actual,
                           TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      continue;
    }

    do
    {
      n = esp8266_ringbuf_read(&s_ring, tmp, sizeof(tmp));
      if (n > 0U)
      {
        LOG_INFO("UART", "UART RX %lu", (unsigned long)n);
        esp8266_at_core_on_rx(tmp, (uint16_t)n);
      }
    } while (n > 0U);

    while (esp8266_at_get_urc(&urc, TX_NO_WAIT) == TX_SUCCESS)
    {
      process_one_urc(urc);
      esp8266_at_release_urc(urc);
    }
  }
}

static void mqtt_thread_entry(ULONG arg)
{
  ULONG actual;
  UINT s;
  (void)arg;

  for (;;)
  {
    switch (s_link_state)
    {
      case ESP_LINK_BOOT:
        s = wifi_basic_init();
        if (s != TX_SUCCESS)
        {
          LOG_ERROR("WIFI", "basic init failed, retry");
          tx_thread_sleep(ESP8266_RECONNECT_PERIOD);
          break;
        }
        s_link_state = ESP_LINK_WIFI_JOINING;
        break;

      case ESP_LINK_WIFI_JOINING:
        s = wifi_join_ap();
        if (s != TX_SUCCESS)
        {
          LOG_ERROR("WIFI", "join AP failed, retry");
          tx_thread_sleep(ESP8266_RECONNECT_PERIOD);
          break;
        }
        s_link_state = ESP_LINK_WIFI_OK;
        break;

      case ESP_LINK_WIFI_OK:
      case ESP_LINK_MQTT_CONNECTING:
        s_link_state = ESP_LINK_MQTT_CONNECTING;
        s = mqtt_online_init();
        if (s != TX_SUCCESS)
        {
          LOG_ERROR("MQTT", "connect failed, retry");
          tx_thread_sleep(ESP8266_RECONNECT_PERIOD);
          break;
        }
        s_link_state = ESP_LINK_MQTT_OK;
        break;

      case ESP_LINK_MQTT_OK:
        if (tx_event_flags_get(&s_drv_flags,
                               ESP8266_DRV_EVT_RECONNECT,
                               TX_OR_CLEAR,
                               &actual,
                               TX_TIMER_TICKS_PER_SECOND) == TX_SUCCESS)
        {
          LOG_WARN("MQTT", "MQTT RECONNECT");
          s_link_state = ESP_LINK_RECOVER;
        }
        break;

      case ESP_LINK_RECOVER:
        (void)esp8266_mqtt_disconnect();
        tx_thread_sleep(ESP8266_RECONNECT_PERIOD);
        s_link_state = ESP_LINK_WIFI_JOINING;
        break;

      default:
        s_link_state = ESP_LINK_BOOT;
        break;
    }
  }
}

static void heartbeat_thread_entry(ULONG arg)
{
  (void)arg;

  for (;;)
  {
    if (s_link_state == ESP_LINK_MQTT_OK)
    {
      if (esp8266_mqtt_publish(MQTT_PUB_TOPIC, "heartbeat", MQTT_QOS0) == TX_SUCCESS)
      {
        LOG_INFO("MQTT", "MQTT PUB OK");
      }
    }
    tx_thread_sleep(ESP8266_HEARTBEAT_PERIOD);
  }
}

/* =============================
 * Public APIs
 * ============================= */
UINT esp8266_cloud_publish(const char *topic, const char *payload)
{
  if ((topic == NULL) || (payload == NULL))
  {
    return TX_PTR_ERROR;
  }

  if (s_link_state != ESP_LINK_MQTT_OK)
  {
    return TX_NOT_AVAILABLE;
  }

  return esp8266_mqtt_publish(topic, payload, MQTT_QOS0);
}

UINT esp8266_service_init(TX_BYTE_POOL *pool)
{
  UINT s;
  (void)pool;

  esp8266_ringbuf_init(&s_ring, s_ring_mem, sizeof(s_ring_mem));

  s = tx_event_flags_create(&s_drv_flags, "esp8266_drv_flags");
  if (s != TX_SUCCESS) return s;

  s = tx_mutex_create(&s_tx_mutex, "esp8266_tx_mutex", TX_NO_INHERIT);
  if (s != TX_SUCCESS) return s;

  s = esp8266_at_core_init();
  if (s != TX_SUCCESS) return s;

  s = esp8266_mqtt_init();
  if (s != TX_SUCCESS) return s;

  s = uart_dma_start();
  if (s != TX_SUCCESS)
  {
    LOG_ERROR("UART", "UART DMA start failed");
    return s;
  }

  s = tx_thread_create(&s_rx_thread,
                       "ESP8266_RX_thread",
                       rx_thread_entry,
                       0,
                       s_rx_stack,
                       sizeof(s_rx_stack),
                       ESP8266_RX_THREAD_PRIO,
                       ESP8266_RX_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);
  if (s != TX_SUCCESS) return s;

  s = tx_thread_create(&s_mqtt_thread,
                       "MQTT_BIZ_thread",
                       mqtt_thread_entry,
                       0,
                       s_mqtt_stack,
                       sizeof(s_mqtt_stack),
                       ESP8266_MQTT_THREAD_PRIO,
                       ESP8266_MQTT_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);
  if (s != TX_SUCCESS) return s;

  s = tx_thread_create(&s_heartbeat_thread,
                       "MQTT_HB_thread",
                       heartbeat_thread_entry,
                       0,
                       s_heartbeat_stack,
                       sizeof(s_heartbeat_stack),
                       ESP8266_HEARTBEAT_THREAD_PRIO,
                       ESP8266_HEARTBEAT_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);
  if (s != TX_SUCCESS) return s;

  LOG_INFO("WIFI", "ESP8266 service init done");
  return TX_SUCCESS;
}

/* =============================
 * ISR hooks
 * ============================= */
void esp8266_uart_rx_event_isr(void)
{
  dma_pull_into_ring();
}

void esp8266_uart_rx_half_isr(void)
{
  dma_pull_into_ring();
}

void esp8266_uart_rx_cplt_isr(void)
{
  dma_pull_into_ring();
}

void esp8266_uart_tx_cplt_isr(void)
{
  (void)tx_event_flags_set(&s_drv_flags, ESP8266_DRV_EVT_TX_DONE, TX_OR);
}
