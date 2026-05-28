#include "wifi_8266.h"

#include "main.h"
#include "sensor_data.h"
#include "usart.h"
#include "wifi_8266_config.h"
#include <stdio.h>
#include <string.h>

#define WIFI8266_RX_RING_SIZE 512U
#define WIFI8266_RESP_BUF_SIZE 768U
#define WIFI8266_CMD_BUF_SIZE 512U
#define WIFI8266_JSON_BUF_SIZE 256U

static uint8_t wifi8266_rx_byte;
static uint8_t wifi8266_rx_ring[WIFI8266_RX_RING_SIZE];
static volatile uint16_t wifi8266_rx_head;
static volatile uint16_t wifi8266_rx_tail;
static UINT wifi8266_mqtt_connected;

volatile UINT wifi8266_blink_thread_count;
volatile UINT wifi8266_log_thread_count;
volatile UINT wifi8266_service_thread_count;
volatile UINT wifi8266_rx_overflow_count;
volatile UINT wifi8266_publish_count;
volatile UINT wifi8266_command_count;

static UINT wifi8266_send_cmd(const char *cmd);
static UINT wifi8266_send_cmd_no_flush(const char *cmd);
static UINT wifi8266_wait_for(const char *ok_token, const char *fail_token, ULONG timeout_ticks, UINT quiet_timeout);
static UINT wifi8266_wait_for_token(const char *token, ULONG timeout_ticks);
static UINT wifi8266_rx_pop(uint8_t *byte);
static void wifi8266_rx_flush(void);
static UINT wifi8266_connect_wifi(void);
static UINT wifi8266_connect_mqtt(void);
static UINT wifi8266_subscribe_command(void);
static UINT wifi8266_publish_status(const char *state);
static UINT wifi8266_publish_telemetry(const sensor_data_t *data);
static UINT wifi8266_mqtt_publish(const char *topic, const char *payload);
static void wifi8266_monitor_urc(UINT quiet_timeout);
static void wifi8266_handle_subrecv(const char *response);

void wifi8266_blink_thread_entry(ULONG thread_input)
{
  (void)thread_input;

  for (;;)
  {
    wifi8266_blink_thread_count++;
    HAL_GPIO_TogglePin(KEEP_GPIO_Port, KEEP_Pin);
    tx_thread_sleep(500U);
  }
}

void wifi8266_log_thread_entry(ULONG thread_input)
{
  (void)thread_input;

  for (;;)
  {
    wifi8266_log_thread_count++;
    tx_thread_sleep(2000U);
  }
}

void wifi8266_service_thread_entry(ULONG thread_input)
{
  sensor_data_t sensor_data;

  (void)thread_input;

  tx_thread_sleep(1500U);
  wifi8266_debug_write("[ESP8266] HiveMQ telemetry service start\r\n");

  (void)sensor_data_init();
  (void)HAL_UART_Receive_IT(&huart1, &wifi8266_rx_byte, 1U);

  for (;;)
  {
    wifi8266_service_thread_count++;

    if (wifi8266_connect_wifi() != TX_SUCCESS)
    {
      wifi8266_debug_write("[ESP8266] WIFI connect failed, retry later\r\n");
      tx_thread_sleep(5000U);
      continue;
    }

    if (wifi8266_connect_mqtt() != TX_SUCCESS)
    {
      wifi8266_debug_write("[ESP8266] MQTT connect failed, retry later\r\n");
      tx_thread_sleep(5000U);
      continue;
    }

    if (wifi8266_subscribe_command() != TX_SUCCESS)
    {
      wifi8266_debug_write("[ESP8266] command subscribe failed, retry later\r\n");
      tx_thread_sleep(3000U);
      continue;
    }

    (void)wifi8266_publish_status("online");

    while (wifi8266_mqtt_connected == TX_TRUE)
    {
      if (sensor_data_read(&sensor_data) == TX_SUCCESS)
      {
        if (wifi8266_publish_telemetry(&sensor_data) == TX_SUCCESS)
        {
          wifi8266_publish_count++;
        }
        else
        {
          wifi8266_debug_write("[ESP8266] telemetry publish failed\r\n");
          wifi8266_mqtt_connected = TX_FALSE;
          break;
        }
      }

      wifi8266_monitor_urc(TX_TRUE);
      tx_thread_sleep(WIFI8266_TELEMETRY_PERIOD_TICKS);
    }

    wifi8266_debug_write("[ESP8266] MQTT reconnect\r\n");
    tx_thread_sleep(3000U);
  }
}

void wifi8266_debug_write(const char *text)
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

static UINT wifi8266_connect_wifi(void)
{
  wifi8266_debug_write("\r\n[ESP8266] STEP WIFI: AT\r\n");
  if ((wifi8266_send_cmd("AT\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 200U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("\r\n[ESP8266] STEP WIFI: ATE0\r\n");
  if ((wifi8266_send_cmd("ATE0\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 200U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("\r\n[ESP8266] STEP WIFI: CWMODE STA\r\n");
  if ((wifi8266_send_cmd("AT+CWMODE=1\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 300U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("\r\n[ESP8266] STEP WIFI: JOIN\r\n");
  if ((wifi8266_send_cmd("AT+CWJAP=\"" WIFI8266_WIFI_SSID "\",\"" WIFI8266_WIFI_PASS "\"\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 20000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("[ESP8266] WIFI CONNECTED\r\n");
  return TX_SUCCESS;
}

static UINT wifi8266_connect_mqtt(void)
{
  wifi8266_mqtt_connected = TX_FALSE;

  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: CLEAN\r\n");
  (void)wifi8266_send_cmd("AT+MQTTCLEAN=0\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 2000U, TX_TRUE);
  tx_thread_sleep(500U);

  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: USERCFG\r\n");
  if ((wifi8266_send_cmd("AT+MQTTUSERCFG=0," WIFI8266_MQTT_SCHEME ",\"" WIFI8266_MQTT_CLIENT_ID "\",\"" WIFI8266_MQTT_USERNAME "\",\"" WIFI8266_MQTT_PASSWORD "\",0,0,\"\"\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 3000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: CONN\r\n");
  if ((wifi8266_send_cmd("AT+MQTTCONN=0,\"" WIFI8266_MQTT_HOST "\"," WIFI8266_MQTT_PORT ",1\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("+MQTTCONNECTED", "ERROR", 15000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  (void)wifi8266_wait_for("OK", "ERROR", 300U, TX_TRUE);
  wifi8266_mqtt_connected = TX_TRUE;
  wifi8266_debug_write("[ESP8266] MQTT CONNECTED\r\n");

  return TX_SUCCESS;
}

static UINT wifi8266_subscribe_command(void)
{
  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: SUB COMMAND\r\n");
  if ((wifi8266_send_cmd("AT+MQTTSUB=0,\"" WIFI8266_TOPIC_COMMAND "\",0\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 3000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("[ESP8266] MQTT SUB COMMAND OK\r\n");
  return TX_SUCCESS;
}

static UINT wifi8266_publish_status(const char *state)
{
  char payload[WIFI8266_JSON_BUF_SIZE];

  if (state == TX_NULL)
  {
    state = "unknown";
  }

  (void)snprintf(payload,
                 sizeof(payload),
                 "{\"deviceId\":\"%s\",\"state\":\"%s\",\"tick\":%lu}",
                 WIFI8266_MQTT_CLIENT_ID,
                 state,
                 (unsigned long)tx_time_get());

  return wifi8266_mqtt_publish(WIFI8266_TOPIC_STATUS, payload);
}

static UINT wifi8266_publish_telemetry(const sensor_data_t *data)
{
  char payload[WIFI8266_JSON_BUF_SIZE];

  if (data == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  (void)snprintf(payload,
                 sizeof(payload),
                 "{\"deviceId\":\"%s\",\"seq\":%lu,\"tick\":%lu,\"temperature\":%ld.%02ld,\"humidity\":%ld.%02ld,\"raw\":%u,\"valid\":%u}",
                 WIFI8266_MQTT_CLIENT_ID,
                 (unsigned long)data->sequence,
                 (unsigned long)data->tick,
                 (long)(data->temperature_c_x100 / 100),
                 (long)(data->temperature_c_x100 >= 0 ? data->temperature_c_x100 % 100 : -(data->temperature_c_x100 % 100)),
                 (long)(data->humidity_rh_x100 / 100),
                 (long)(data->humidity_rh_x100 >= 0 ? data->humidity_rh_x100 % 100 : -(data->humidity_rh_x100 % 100)),
                 (unsigned int)data->raw_value,
                 (unsigned int)data->valid);

  return wifi8266_mqtt_publish(WIFI8266_TOPIC_TELEMETRY, payload);
}

static UINT wifi8266_mqtt_publish(const char *topic, const char *payload)
{
  char cmd[WIFI8266_CMD_BUF_SIZE];
  uint16_t payload_len;

  if ((topic == TX_NULL) || (payload == TX_NULL))
  {
    return TX_PTR_ERROR;
  }

  payload_len = (uint16_t)strlen(payload);

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTPUBRAW=0,\"%s\",%u,0,0\r\n",
                 topic,
                 (unsigned int)payload_len);

  if ((wifi8266_send_cmd(cmd) != TX_SUCCESS) ||
      (wifi8266_wait_for(">", "ERROR", 3000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("[ESP8266] RAW: ");
  wifi8266_debug_write(payload);
  wifi8266_debug_write("\r\n");

  if (HAL_UART_Transmit(&huart1,
                        (uint8_t *)payload,
                        payload_len,
                        1000U) != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  if (wifi8266_wait_for("+MQTTPUB:OK", "ERROR", 10000U, TX_FALSE) != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
}

static UINT wifi8266_send_cmd(const char *cmd)
{
  wifi8266_rx_flush();
  return wifi8266_send_cmd_no_flush(cmd);
}

static UINT wifi8266_send_cmd_no_flush(const char *cmd)
{
  if (cmd == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  wifi8266_debug_write("[ESP8266] TX: ");
  wifi8266_debug_write(cmd);

  if (HAL_UART_Transmit(&huart1,
                        (uint8_t *)cmd,
                        (uint16_t)strlen(cmd),
                        300U) != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
}

static UINT wifi8266_wait_for(const char *ok_token, const char *fail_token, ULONG timeout_ticks, UINT quiet_timeout)
{
  char response[WIFI8266_RESP_BUF_SIZE];
  ULONG start_tick;
  uint16_t response_len = 0U;
  uint8_t byte;

  response[0] = '\0';
  start_tick = tx_time_get();

  while ((tx_time_get() - start_tick) < timeout_ticks)
  {
    while (wifi8266_rx_pop(&byte) == TX_SUCCESS)
    {
      (void)HAL_UART_Transmit(&huart3, &byte, 1U, 20U);

      if (response_len < (WIFI8266_RESP_BUF_SIZE - 1U))
      {
        response[response_len++] = (char)byte;
        response[response_len] = '\0';
      }

      wifi8266_handle_subrecv(response);

      if ((strstr(response, "WIFI DISCONNECT") != TX_NULL) ||
          (strstr(response, "MQTTDISCONNECTED") != TX_NULL) ||
          (strstr(response, "MQTT CLOSED") != TX_NULL))
      {
        wifi8266_mqtt_connected = TX_FALSE;
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

  if (quiet_timeout == TX_FALSE)
  {
    wifi8266_debug_write("\r\n[ESP8266] wait timeout\r\n");
  }

  return TX_NOT_DONE;
}

static UINT wifi8266_wait_for_token(const char *token, ULONG timeout_ticks)
{
  return wifi8266_wait_for(token, TX_NULL, timeout_ticks, TX_TRUE);
}

static void wifi8266_monitor_urc(UINT quiet_timeout)
{
  (void)wifi8266_wait_for_token("+MQTTSUBRECV", quiet_timeout == TX_TRUE ? 100U : 1000U);
}

static void wifi8266_handle_subrecv(const char *response)
{
  if ((response != TX_NULL) && (strstr(response, "+MQTTSUBRECV") != TX_NULL))
  {
    wifi8266_command_count++;
  }
}

static UINT wifi8266_rx_pop(uint8_t *byte)
{
  if (wifi8266_rx_tail == wifi8266_rx_head)
  {
    return TX_NO_INSTANCE;
  }

  *byte = wifi8266_rx_ring[wifi8266_rx_tail];
  wifi8266_rx_tail = (uint16_t)((wifi8266_rx_tail + 1U) % WIFI8266_RX_RING_SIZE);

  return TX_SUCCESS;
}

static void wifi8266_rx_flush(void)
{
  uint8_t byte;

  while (wifi8266_rx_pop(&byte) == TX_SUCCESS)
  {
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t next_head;

  if (huart->Instance == USART1)
  {
    next_head = (uint16_t)((wifi8266_rx_head + 1U) % WIFI8266_RX_RING_SIZE);

    if (next_head != wifi8266_rx_tail)
    {
      wifi8266_rx_ring[wifi8266_rx_head] = wifi8266_rx_byte;
      wifi8266_rx_head = next_head;
    }
    else
    {
      wifi8266_rx_overflow_count++;
    }

    (void)HAL_UART_Receive_IT(&huart1, &wifi8266_rx_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    (void)HAL_UART_Receive_IT(&huart1, &wifi8266_rx_byte, 1U);
  }
}
