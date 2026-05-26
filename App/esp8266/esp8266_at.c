#include "esp8266_at.h"

#include <stdio.h>
#include <string.h>

#define URC_QUEUE_WORDS  ESP8266_AT_EVENT_POOL_SIZE

static TX_QUEUE s_urc_q;
static ULONG s_urc_q_mem[URC_QUEUE_WORDS];
static TX_EVENT_FLAGS_GROUP s_at_flags;

static esp8266_line_parser_t s_line_parser;
static esp8266_urc_t s_urc_pool[ESP8266_AT_EVENT_POOL_SIZE];
static uint8_t s_urc_used[ESP8266_AT_EVENT_POOL_SIZE];

static esp8266_urc_t *urc_alloc(void)
{
  UINT i;
  TX_INTERRUPT_SAVE_AREA

  TX_DISABLE
  for (i = 0U; i < ESP8266_AT_EVENT_POOL_SIZE; i++)
  {
    if (s_urc_used[i] == 0U)
    {
      s_urc_used[i] = 1U;
      TX_RESTORE
      return &s_urc_pool[i];
    }
  }
  TX_RESTORE

  return NULL;
}

void esp8266_at_release_urc(esp8266_urc_t *urc)
{
  UINT i;
  TX_INTERRUPT_SAVE_AREA

  if (urc == NULL)
  {
    return;
  }

  TX_DISABLE
  for (i = 0U; i < ESP8266_AT_EVENT_POOL_SIZE; i++)
  {
    if (&s_urc_pool[i] == urc)
    {
      s_urc_used[i] = 0U;
      break;
    }
  }
  TX_RESTORE
}

static void set_flag_by_urc(esp8266_urc_type_t type)
{
  ULONG flg = 0UL;

  switch (type)
  {
    case ESP8266_URC_OK: flg = ESP8266_AT_FLG_OK; break;
    case ESP8266_URC_ERROR: flg = ESP8266_AT_FLG_ERROR; break;
    case ESP8266_URC_FAIL: flg = ESP8266_AT_FLG_FAIL; break;
    case ESP8266_URC_BUSY: flg = ESP8266_AT_FLG_BUSY; break;
    case ESP8266_URC_WIFI_CONNECTED: flg = ESP8266_AT_FLG_WIFI_CONNECTED; break;
    case ESP8266_URC_WIFI_DISCONNECT: flg = ESP8266_AT_FLG_WIFI_DISCONNECT; break;
    case ESP8266_URC_WIFI_GOT_IP: flg = ESP8266_AT_FLG_WIFI_GOT_IP; break;
    case ESP8266_URC_MQTT_CONNECTED: flg = ESP8266_AT_FLG_MQTT_CONNECTED; break;
    case ESP8266_URC_MQTT_DISCONNECTED: flg = ESP8266_AT_FLG_MQTT_DISCONNECTED; break;
    case ESP8266_URC_CLOSED: flg = ESP8266_AT_FLG_CLOSED; break;
    case ESP8266_URC_READY: flg = ESP8266_AT_FLG_READY; break;
    case ESP8266_URC_PROMPT: flg = ESP8266_AT_FLG_PROMPT; break;
    default: break;
  }

  if (flg != 0UL)
  {
    (void)tx_event_flags_set(&s_at_flags, flg, TX_OR);
  }
}

UINT esp8266_at_core_init(void)
{
  UINT status;

  memset(s_urc_used, 0, sizeof(s_urc_used));
  esp8266_parser_init(&s_line_parser);

  status = tx_queue_create(&s_urc_q, "esp8266_urc_q", TX_1_ULONG, s_urc_q_mem, sizeof(s_urc_q_mem));
  if (status != TX_SUCCESS)
  {
    return status;
  }

  status = tx_event_flags_create(&s_at_flags, "esp8266_at_flags");
  if (status != TX_SUCCESS)
  {
    return status;
  }

  return TX_SUCCESS;
}

void esp8266_at_core_on_rx(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  bool ready;
  esp8266_urc_t temp;
  esp8266_urc_t *evt;
  ULONG ptr;

  if ((data == NULL) || (len == 0U))
  {
    return;
  }

  for (i = 0U; i < len; i++)
  {
    if (!esp8266_parser_feed(&s_line_parser, data[i], &temp, &ready))
    {
      continue;
    }

    if (!ready)
    {
      continue;
    }

    evt = urc_alloc();
    if (evt == NULL)
    {
      printf("[E][AT] URC pool exhausted\r\n");
      continue;
    }

    *evt = temp;
    evt->tick = tx_time_get();

    set_flag_by_urc(evt->type);

    ptr = (ULONG)evt;
    if (tx_queue_send(&s_urc_q, &ptr, TX_NO_WAIT) != TX_SUCCESS)
    {
      printf("[E][AT] URC queue full\r\n");
      esp8266_at_release_urc(evt);
    }
  }
}

UINT esp8266_at_wait_result(esp8266_at_result_t *res,
                            ULONG wait_ticks,
                            ULONG expect_flags,
                            ULONG *actual_flags)
{
  ULONG actual = 0UL;
  UINT status;

  if (res == NULL)
  {
    return TX_PTR_ERROR;
  }

  memset(res, 0, sizeof(*res));

  status = tx_event_flags_get(&s_at_flags,
                              expect_flags,
                              TX_OR_CLEAR,
                              &actual,
                              wait_ticks);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  res->ok = ((actual & ESP8266_AT_FLG_OK) != 0UL);
  res->error = ((actual & ESP8266_AT_FLG_ERROR) != 0UL);
  res->fail = ((actual & ESP8266_AT_FLG_FAIL) != 0UL);
  res->busy = ((actual & ESP8266_AT_FLG_BUSY) != 0UL);
  res->wifi_connected = ((actual & ESP8266_AT_FLG_WIFI_CONNECTED) != 0UL);
  res->wifi_disconnect = ((actual & ESP8266_AT_FLG_WIFI_DISCONNECT) != 0UL);
  res->wifi_got_ip = ((actual & ESP8266_AT_FLG_WIFI_GOT_IP) != 0UL);
  res->mqtt_connected = ((actual & ESP8266_AT_FLG_MQTT_CONNECTED) != 0UL);
  res->mqtt_disconnected = ((actual & ESP8266_AT_FLG_MQTT_DISCONNECTED) != 0UL);
  res->closed = ((actual & ESP8266_AT_FLG_CLOSED) != 0UL);
  res->ready = ((actual & ESP8266_AT_FLG_READY) != 0UL);
  res->prompt = ((actual & ESP8266_AT_FLG_PROMPT) != 0UL);

  if (actual_flags != NULL)
  {
    *actual_flags = actual;
  }

  return TX_SUCCESS;
}

UINT esp8266_at_get_urc(esp8266_urc_t **urc, ULONG wait)
{
  ULONG ptr;
  UINT status;

  if (urc == NULL)
  {
    return TX_PTR_ERROR;
  }

  status = tx_queue_receive(&s_urc_q, &ptr, wait);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  *urc = (esp8266_urc_t *)ptr;
  return TX_SUCCESS;
}

void esp8266_at_reset_flags(void)
{
  ULONG tmp;
  (void)tx_event_flags_get(&s_at_flags, 0xFFFFFFFFUL, TX_OR_CLEAR, &tmp, TX_NO_WAIT);
}

TX_EVENT_FLAGS_GROUP *esp8266_at_flags_group(void)
{
  return &s_at_flags;
}
