#include "wifi_8266.h"

#include "main.h"
#include "sensor_data.h"
#include "usart.h"
#include "wifi_8266_config.h"
#include "wifi_8266_provision.h"
#include <stdio.h>
#include <string.h>

#define WIFI8266_RX_RING_SIZE 512U
#define WIFI8266_RESP_BUF_SIZE 768U
#define WIFI8266_CMD_BUF_SIZE 512U
#define WIFI8266_JSON_BUF_SIZE 768U
#define WIFI8266_HTTP_BUF_SIZE 2048U
#define WIFI8266_PROV_AP_SSID "STM32_WIFI_SETUP"
#define WIFI8266_PROV_AP_PASS "12345678"
#define WIFI8266_PROV_TIMEOUT_TICKS 180000U
#define WIFI8266_AT_SYNC_RETRY 8U
#define WIFI8266_MQTT_RETRY_BEFORE_WIFI_REJOIN 3U
#define WIFI8266_WIFI_READY_DELAY_TICKS 3000U
#define WIFI8266_MQTT_CONNECT_TIMEOUT_TICKS 30000U

typedef enum
{
  WIFI8266_STATE_PROVISION = 0,
  WIFI8266_STATE_WIFI_CONNECT,
  WIFI8266_STATE_MQTT_CONNECT,
  WIFI8266_STATE_MQTT_SUBSCRIBE,
  WIFI8266_STATE_RUN,
  WIFI8266_STATE_RESET
} wifi8266_state_t;

static uint8_t wifi8266_rx_byte;
static uint8_t wifi8266_rx_ring[WIFI8266_RX_RING_SIZE];
static volatile uint16_t wifi8266_rx_head;
static volatile uint16_t wifi8266_rx_tail;
static UINT wifi8266_wifi_connected;
static UINT wifi8266_mqtt_connected;
static char wifi8266_http_request[WIFI8266_HTTP_BUF_SIZE];
static wifi8266_provision_config_t wifi8266_wifi_config;

volatile UINT wifi8266_blink_thread_count;
volatile UINT wifi8266_log_thread_count;
volatile UINT wifi8266_service_thread_count;
volatile UINT wifi8266_rx_overflow_count;
volatile UINT wifi8266_publish_count;
volatile UINT wifi8266_command_count;

static UINT wifi8266_send_cmd(const char *cmd);
static UINT wifi8266_send_cmd_hidden(const char *log_text, const char *cmd);
static UINT wifi8266_send_cmd_no_flush(const char *cmd);
static UINT wifi8266_wait_for(const char *ok_token, const char *fail_token, ULONG timeout_ticks, UINT quiet_timeout);
static UINT wifi8266_wait_for_token(const char *token, ULONG timeout_ticks);
static UINT wifi8266_rx_pop(uint8_t *byte);
static void wifi8266_rx_flush(void);
static UINT wifi8266_connect_wifi(void);
static UINT wifi8266_connect_mqtt(void);
static UINT wifi8266_subscribe_command(void);
static UINT wifi8266_sync_at(uint8_t retry_count, ULONG timeout_ticks);
static void wifi8266_reset_module(void);
static UINT wifi8266_web_provision_run(wifi8266_provision_config_t *config);
static UINT wifi8266_http_wait_request(char *request, uint16_t max_len, ULONG timeout_ticks);
static UINT wifi8266_http_send_response(uint8_t link_id, const char *body);
static UINT wifi8266_http_parse_link_id(const char *request, uint8_t *link_id);
static UINT wifi8266_http_extract_credentials(const char *request, wifi8266_provision_config_t *config);
static uint8_t wifi8266_url_decode(char *dst, uint16_t dst_len, const char *src, const char *src_end);
static int wifi8266_hex_value(char ch);
static const char wifi8266_setup_page[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html\r\n"
  "Cache-Control: no-store\r\n"
  "Connection: close\r\n\r\n"
  "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<title>STM32 WiFi Setup</title>"
  "<style>"
  "body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
  "font-family:Segoe UI,Arial,sans-serif;background:linear-gradient(135deg,#0f766e,#f59e0b);}"
  ".card{width:88%;max-width:380px;background:#fff;border-radius:22px;padding:28px;"
  "box-shadow:0 20px 60px rgba(0,0,0,.25)}"
  "h2{margin:0 0 8px;color:#123;font-size:26px}.sub{margin:0 0 22px;color:#667;font-size:14px}"
  "label{display:block;margin:14px 0 6px;color:#234;font-weight:600}"
  "input{width:100%;box-sizing:border-box;padding:13px 14px;border:1px solid #d6dde5;"
  "border-radius:12px;font-size:16px;outline:none}"
  "input:focus{border-color:#0f766e;box-shadow:0 0 0 3px rgba(15,118,110,.16)}"
  "button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:14px;"
  "background:#0f766e;color:#fff;font-size:17px;font-weight:700}"
  ".hint{margin-top:16px;text-align:center;color:#778;font-size:12px}"
  "</style></head><body><div class=\"card\">"
  "<h2>STM32 WiFi Setup</h2><p class=\"sub\">Connect this device to your WiFi network.</p>"
  "<form method=\"POST\" action=\"/set\">"
  "<label>WiFi SSID</label><input name=\"ssid\" placeholder=\"Your WiFi name\" required>"
  "<label>Password</label><input name=\"pass\" type=\"password\" placeholder=\"WiFi password\" required>"
  "<button type=\"submit\">Connect</button></form>"
  "<div class=\"hint\">AP: STM32_WIFI_SETUP / http://192.168.4.1</div>"
  "</div></body></html>";

static const char wifi8266_setup_ok_page[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html\r\n"
  "Cache-Control: no-store\r\n"
  "Connection: close\r\n\r\n"
  "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
  "font-family:Segoe UI,Arial,sans-serif;background:#ecfdf5}.card{text-align:center;background:white;"
  "border-radius:22px;padding:34px;box-shadow:0 18px 50px rgba(0,0,0,.12)}h2{color:#0f766e}</style>"
  "</head><body><div class=\"card\"><h2>WiFi saved</h2><p>STM32 is connecting now...</p></div></body></html>";
static UINT wifi8266_publish_nodes_telemetry(const sensor_data_t *data);
static UINT wifi8266_mqtt_publish_ex(const char *topic, const char *payload, UINT retain);
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
  uint8_t mqtt_retry_count = 0U;
  wifi8266_state_t state = WIFI8266_STATE_PROVISION;

  (void)thread_input;

  tx_thread_sleep(1500U);
  wifi8266_debug_write("[ESP8266] HiveMQ telemetry service start\r\n");

  (void)sensor_data_init();
  (void)HAL_UART_Receive_IT(&huart1, &wifi8266_rx_byte, 1U);

  for (;;)
  {
    wifi8266_service_thread_count++;

    switch (state)
    {
      case WIFI8266_STATE_PROVISION:
        (void)wifi8266_web_provision_run(&wifi8266_wifi_config);
        state = WIFI8266_STATE_WIFI_CONNECT;
        break;

      case WIFI8266_STATE_WIFI_CONNECT:
        if (wifi8266_connect_wifi() == TX_SUCCESS)
        {
          state = WIFI8266_STATE_MQTT_CONNECT;
        }
        else
        {
          wifi8266_debug_write("[ESP8266] WIFI connect failed, retry provisioning later\r\n");
          state = WIFI8266_STATE_PROVISION;
          tx_thread_sleep(5000U);
        }
        break;

      case WIFI8266_STATE_MQTT_CONNECT:
        if (wifi8266_connect_mqtt() == TX_SUCCESS)
        {
          mqtt_retry_count = 0U;
          state = WIFI8266_STATE_MQTT_SUBSCRIBE;
        }
        else
        {
          mqtt_retry_count++;
          wifi8266_debug_write("[ESP8266] MQTT connect failed\r\n");

          if (mqtt_retry_count >= WIFI8266_MQTT_RETRY_BEFORE_WIFI_REJOIN)
          {
            mqtt_retry_count = 0U;
            state = WIFI8266_STATE_RESET;
          }
          else
          {
            tx_thread_sleep(5000U);
          }
        }
        break;

      case WIFI8266_STATE_MQTT_SUBSCRIBE:
        if (wifi8266_subscribe_command() == TX_SUCCESS)
        {
          state = WIFI8266_STATE_RUN;
        }
        else
        {
          wifi8266_debug_write("[ESP8266] command subscribe failed, reconnect MQTT later\r\n");
          wifi8266_mqtt_connected = TX_FALSE;
          state = WIFI8266_STATE_MQTT_CONNECT;
          tx_thread_sleep(3000U);
        }
        break;

      case WIFI8266_STATE_RUN:
        if (wifi8266_wifi_connected != TX_TRUE)
        {
          wifi8266_debug_write("[ESP8266] WiFi reconnect\r\n");
          wifi8266_mqtt_connected = TX_FALSE;
          state = WIFI8266_STATE_WIFI_CONNECT;
          tx_thread_sleep(3000U);
          break;
        }

        if (wifi8266_mqtt_connected != TX_TRUE)
        {
          wifi8266_debug_write("[ESP8266] MQTT reconnect\r\n");
          state = WIFI8266_STATE_MQTT_CONNECT;
          tx_thread_sleep(3000U);
          break;
        }

        if (sensor_data_read(&sensor_data) == TX_SUCCESS)
        {
          if (wifi8266_publish_nodes_telemetry(&sensor_data) == TX_SUCCESS)
          {
            wifi8266_publish_count++;
          }
          else
          {
            wifi8266_debug_write("[ESP8266] telemetry publish failed\r\n");
            wifi8266_mqtt_connected = TX_FALSE;
            state = WIFI8266_STATE_MQTT_CONNECT;
          }
        }

        wifi8266_monitor_urc(TX_TRUE);
        tx_thread_sleep(WIFI8266_TELEMETRY_PERIOD_TICKS);
        break;

      case WIFI8266_STATE_RESET:
      default:
        wifi8266_debug_write("[ESP8266] MQTT failed too many times, reset ESP8266\r\n");
        wifi8266_reset_module();
        state = WIFI8266_STATE_WIFI_CONNECT;
        tx_thread_sleep(3000U);
        break;
    }
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
  const char *ssid = WIFI8266_WIFI_SSID;
  const char *password = WIFI8266_WIFI_PASS;
  char cmd[WIFI8266_CMD_BUF_SIZE];

  if (wifi8266_wifi_config.valid != 0U)
  {
    ssid = wifi8266_wifi_config.ssid;
    password = wifi8266_wifi_config.password;
  }

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
  wifi8266_debug_write("[ESP8266] WIFI SSID: ");
  wifi8266_debug_write(ssid);
  wifi8266_debug_write("\r\n");

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+CWJAP=\"%s\",\"%s\"\r\n",
                 ssid,
                 password);

  if ((wifi8266_send_cmd_hidden("[ESP8266] TX: AT+CWJAP=<hidden>\r\n", cmd) != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 20000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("[ESP8266] WIFI CONNECTED\r\n");
  wifi8266_wifi_connected = TX_TRUE;
  tx_thread_sleep(WIFI8266_WIFI_READY_DELAY_TICKS);
  return TX_SUCCESS;
}

static UINT wifi8266_connect_mqtt(void)
{
  char cmd[WIFI8266_CMD_BUF_SIZE];

  wifi8266_mqtt_connected = TX_FALSE;

  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: CLEAN\r\n");
  (void)wifi8266_send_cmd("AT+CIPMUX=0\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);

  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: LINK CHECK\r\n");
  (void)wifi8266_send_cmd("AT+CWJAP?\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 2000U, TX_FALSE);
  (void)wifi8266_send_cmd("AT+CIPSTATUS\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 2000U, TX_FALSE);
  tx_thread_sleep(1000U);

  (void)wifi8266_send_cmd("AT+MQTTCLEAN=0\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 2000U, TX_TRUE);
  tx_thread_sleep(1000U);

  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: USERCFG\r\n");
  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTUSERCFG=0,%s,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
                 WIFI8266_MQTT_SCHEME,
                 WIFI8266_MQTT_CLIENT_ID,
                 WIFI8266_MQTT_USERNAME,
                 WIFI8266_MQTT_PASSWORD);
  if ((wifi8266_send_cmd_hidden("[ESP8266] TX: AT+MQTTUSERCFG=<hidden>\r\n", cmd) != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 3000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }
  tx_thread_sleep(500U);

  wifi8266_debug_write("\r\n[ESP8266] STEP MQTT: CONN\r\n");
  if ((wifi8266_send_cmd("AT+MQTTCONN=0,\"" WIFI8266_MQTT_HOST "\"," WIFI8266_MQTT_PORT ",1\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("+MQTTCONNECTED", "ERROR", WIFI8266_MQTT_CONNECT_TIMEOUT_TICKS, TX_FALSE) != TX_SUCCESS))
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

  if ((wifi8266_send_cmd("AT+MQTTSUB=0,\"" WIFI8266_TOPIC_NODE_COMMANDS "\",0\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 3000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("[ESP8266] MQTT SUB COMMAND OK\r\n");
  return TX_SUCCESS;
}

static UINT wifi8266_sync_at(uint8_t retry_count, ULONG timeout_ticks)
{
  uint8_t retry;

  for (retry = 0U; retry < retry_count; retry++)
  {
    (void)HAL_UART_AbortReceive_IT(&huart1);
    wifi8266_rx_head = 0U;
    wifi8266_rx_tail = 0U;
    (void)HAL_UART_Receive_IT(&huart1, &wifi8266_rx_byte, 1U);
    tx_thread_sleep(200U);

    wifi8266_debug_write("[ESP8266] AT sync try\r\n");
    if ((wifi8266_send_cmd("AT\r\n") == TX_SUCCESS) &&
        (wifi8266_wait_for("OK", "ERROR", timeout_ticks, TX_TRUE) == TX_SUCCESS))
    {
      wifi8266_debug_write("[ESP8266] AT sync OK\r\n");
      return TX_SUCCESS;
    }

    tx_thread_sleep(500U);
  }

  wifi8266_debug_write("[ESP8266] AT sync failed, check USART1 wiring/power\r\n");
  return TX_NOT_DONE;
}

static void wifi8266_reset_module(void)
{
  wifi8266_debug_write("[ESP8266] Reset ESP8266 AT stack\r\n");
  wifi8266_wifi_connected = TX_FALSE;
  wifi8266_mqtt_connected = TX_FALSE;

  (void)wifi8266_send_cmd("AT+RST\r\n");
  (void)wifi8266_wait_for("ready", TX_NULL, 8000U, TX_FALSE);

  (void)HAL_UART_AbortReceive_IT(&huart1);
  wifi8266_rx_head = 0U;
  wifi8266_rx_tail = 0U;
  (void)HAL_UART_Receive_IT(&huart1, &wifi8266_rx_byte, 1U);

  tx_thread_sleep(2000U);
}

static UINT wifi8266_web_provision_run(wifi8266_provision_config_t *config)
{
  uint8_t link_id;
  ULONG start_tick;

  if (config == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  (void)memset(config, 0, sizeof(*config));

  wifi8266_debug_write("\r\n[ESP8266] Web WiFi setup start\r\n");
  wifi8266_debug_write("[ESP8266] Connect AP: " WIFI8266_PROV_AP_SSID "\r\n");
  wifi8266_debug_write("[ESP8266] Open: http://192.168.4.1\r\n");

  if (wifi8266_sync_at(WIFI8266_AT_SYNC_RETRY, 500U) != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  (void)wifi8266_send_cmd("ATE0\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 300U, TX_TRUE);

  if ((wifi8266_send_cmd("AT+CWMODE=2\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 500U, TX_FALSE) != TX_SUCCESS))
  {
    wifi8266_debug_write("[ESP8266] CWMODE AP failed\r\n");
    return TX_NOT_DONE;
  }

  if ((wifi8266_send_cmd("AT+CWSAP=\"" WIFI8266_PROV_AP_SSID "\",\"" WIFI8266_PROV_AP_PASS "\",5,3\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 3000U, TX_FALSE) != TX_SUCCESS))
  {
    wifi8266_debug_write("[ESP8266] CWSAP failed\r\n");
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("[ESP8266] SoftAP ready: " WIFI8266_PROV_AP_SSID "\r\n");

  (void)wifi8266_send_cmd("AT+CIPMUX=1\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);

  (void)wifi8266_send_cmd("AT+CIPSERVER=0\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);

  if ((wifi8266_send_cmd("AT+CIPSERVER=1,80\r\n") != TX_SUCCESS) ||
      (wifi8266_wait_for("OK", "ERROR", 1000U, TX_FALSE) != TX_SUCCESS))
  {
    wifi8266_debug_write("[ESP8266] HTTP server start failed\r\n");
    return TX_NOT_DONE;
  }

  wifi8266_debug_write("[ESP8266] HTTP server ready\r\n");

  start_tick = tx_time_get();
  while ((tx_time_get() - start_tick) < WIFI8266_PROV_TIMEOUT_TICKS)
  {
    if (wifi8266_http_wait_request(wifi8266_http_request, sizeof(wifi8266_http_request), 1000U) != TX_SUCCESS)
    {
      continue;
    }

    link_id = 0U;
    (void)wifi8266_http_parse_link_id(wifi8266_http_request, &link_id);

    if (wifi8266_http_extract_credentials(wifi8266_http_request, config) == TX_SUCCESS)
    {
      (void)wifi8266_http_send_response(link_id, wifi8266_setup_ok_page);
      wifi8266_debug_write("[ESP8266] Web WiFi config accepted\r\n");
      (void)wifi8266_send_cmd("AT+CIPSERVER=0\r\n");
      (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);
      (void)wifi8266_send_cmd("AT+CIPMUX=0\r\n");
      (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);
      return TX_SUCCESS;
    }

    (void)wifi8266_http_send_response(link_id, wifi8266_setup_page);
  }

  wifi8266_debug_write("[ESP8266] Web WiFi setup timeout, use default WiFi\r\n");
  (void)wifi8266_send_cmd("AT+CIPSERVER=0\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);
  (void)wifi8266_send_cmd("AT+CIPMUX=0\r\n");
  (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);

  return TX_NOT_DONE;
}

static UINT wifi8266_http_wait_request(char *request, uint16_t max_len, ULONG timeout_ticks)
{
  ULONG start_tick;
  uint16_t len = 0U;
  uint16_t ipd_len = 0U;
  uint16_t ipd_body_len = 0U;
  uint8_t ipd_seen = 0U;
  uint8_t byte;
  const char *ipd;
  const char *colon;
  const char *num;

  if ((request == TX_NULL) || (max_len == 0U))
  {
    return TX_PTR_ERROR;
  }

  request[0] = '\0';
  start_tick = tx_time_get();

  while ((tx_time_get() - start_tick) < timeout_ticks)
  {
    while (wifi8266_rx_pop(&byte) == TX_SUCCESS)
    {
      if (len < (max_len - 1U))
      {
        request[len++] = (char)byte;
        request[len] = '\0';
      }

      if (ipd_seen == 0U)
      {
        ipd = strstr(request, "+IPD,");
        colon = (ipd != TX_NULL) ? strchr(ipd, ':') : TX_NULL;
        if (colon != TX_NULL)
        {
          num = ipd + 5;
          if ((*num >= '0') && (*num <= '4') && (*(num + 1) == ','))
          {
            num += 2;
          }

          while ((num < colon) && (*num >= '0') && (*num <= '9'))
          {
            ipd_len = (uint16_t)((ipd_len * 10U) + (uint16_t)(*num - '0'));
            num++;
          }

          ipd_body_len = (uint16_t)(len - (uint16_t)((colon + 1) - request));
          ipd_seen = 1U;
        }
      }
      else
      {
        colon = strchr(request, ':');
        if (colon != TX_NULL)
        {
          ipd_body_len = (uint16_t)(len - (uint16_t)((colon + 1) - request));
        }
      }

      if ((ipd_seen != 0U) && (ipd_body_len >= ipd_len))
      {
        wifi8266_debug_write("\r\n[ESP8266] HTTP request complete\r\n");
        return TX_SUCCESS;
      }
    }

    tx_thread_sleep(5U);
  }

  return TX_NOT_DONE;
}

static UINT wifi8266_http_send_response(uint8_t link_id, const char *body)
{
  char cmd[64];
  uint16_t body_len;

  if (body == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  body_len = (uint16_t)strlen(body);
  (void)snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u,%u\r\n", (unsigned int)link_id, (unsigned int)body_len);

  if ((wifi8266_send_cmd(cmd) != TX_SUCCESS) ||
      (wifi8266_wait_for(">", "ERROR", 2000U, TX_FALSE) != TX_SUCCESS))
  {
    return TX_NOT_DONE;
  }

  if (HAL_UART_Transmit(&huart1, (uint8_t *)body, body_len, 1000U) != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  (void)wifi8266_wait_for("SEND OK", "ERROR", 3000U, TX_TRUE);

  (void)snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%u\r\n", (unsigned int)link_id);
  (void)wifi8266_send_cmd(cmd);
  (void)wifi8266_wait_for("OK", "ERROR", 1000U, TX_TRUE);

  return TX_SUCCESS;
}

static UINT wifi8266_http_parse_link_id(const char *request, uint8_t *link_id)
{
  const char *ipd;

  if ((request == TX_NULL) || (link_id == TX_NULL))
  {
    return TX_PTR_ERROR;
  }

  ipd = strstr(request, "+IPD,");
  if ((ipd == TX_NULL) || (ipd[5] < '0') || (ipd[5] > '4'))
  {
    *link_id = 0U;
    return TX_NOT_DONE;
  }

  *link_id = (uint8_t)(ipd[5] - '0');
  return TX_SUCCESS;
}

static UINT wifi8266_http_extract_credentials(const char *request, wifi8266_provision_config_t *config)
{
  const char *ssid;
  const char *pass;
  const char *ssid_end;
  const char *pass_end;
  const char *body;

  if ((request == TX_NULL) || (config == TX_NULL))
  {
    return TX_PTR_ERROR;
  }

  body = strstr(request, "\r\n\r\n");
  if ((strstr(request, "POST /set ") != TX_NULL) && (body != TX_NULL))
  {
    body += 4U;
    ssid = strstr(body, "ssid=");
  }
  else
  {
    ssid = strstr(request, "GET /set?ssid=");
    if (ssid != TX_NULL)
    {
      ssid += strlen("GET /set?");
    }
  }

  if (ssid == TX_NULL)
  {
    return TX_NOT_DONE;
  }

  if (strncmp(ssid, "ssid=", strlen("ssid=")) != 0)
  {
    return TX_NOT_DONE;
  }

  ssid += strlen("ssid=");
  pass = strstr(ssid, "&pass=");
  if (pass == TX_NULL)
  {
    return TX_NOT_DONE;
  }

  ssid_end = pass;
  pass += strlen("&pass=");
  pass_end = pass;
  while ((*pass_end != '\0') && (*pass_end != '\r') && (*pass_end != '\n') && (*pass_end != ' '))
  {
    pass_end++;
  }
  if (pass_end == pass)
  {
    return TX_NOT_DONE;
  }

  (void)memset(config, 0, sizeof(*config));
  if ((wifi8266_url_decode(config->ssid, sizeof(config->ssid), ssid, ssid_end) == 0U) ||
      (wifi8266_url_decode(config->password, sizeof(config->password), pass, pass_end) == 0U))
  {
    (void)memset(config, 0, sizeof(*config));
    return TX_NOT_DONE;
  }

  config->valid = 1U;
  return TX_SUCCESS;
}

static uint8_t wifi8266_url_decode(char *dst, uint16_t dst_len, const char *src, const char *src_end)
{
  uint16_t len = 0U;
  int hi;
  int lo;

  if ((dst == TX_NULL) || (src == TX_NULL) || (src_end == TX_NULL) || (dst_len == 0U))
  {
    return 0U;
  }

  while ((src < src_end) && (len < (dst_len - 1U)))
  {
    if ((*src == '%') && ((src + 2) < src_end))
    {
      hi = wifi8266_hex_value(*(src + 1));
      lo = wifi8266_hex_value(*(src + 2));
      if ((hi >= 0) && (lo >= 0))
      {
        dst[len++] = (char)((hi << 4) | lo);
        src += 3;
        continue;
      }
    }

    dst[len++] = (*src == '+') ? ' ' : *src;
    src++;
  }

  dst[len] = '\0';
  return (uint8_t)len;
}

static int wifi8266_hex_value(char ch)
{
  if ((ch >= '0') && (ch <= '9'))
  {
    return ch - '0';
  }

  if ((ch >= 'a') && (ch <= 'f'))
  {
    return ch - 'a' + 10;
  }

  if ((ch >= 'A') && (ch <= 'F'))
  {
    return ch - 'A' + 10;
  }

  return -1;
}

static UINT wifi8266_publish_nodes_telemetry(const sensor_data_t *data)
{
  char payload[WIFI8266_JSON_BUF_SIZE];

  if (data == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  (void)snprintf(payload,
                 sizeof(payload),
                 "{\"deviceId\":\"%s\",\"seq\":%lu,\"tick\":%lu,\"nodes\":["
                 "{\"nodeId\":\"%s\",\"type\":\"%s\",\"Temp\":%ld.%02ld,\"Hum\":%ld.%02ld,\"value\":0,\"valid\":%u},"
                 "{\"nodeId\":\"%s\",\"type\":\"%s\",\"Temp\":0.00,\"Hum\":0.00,\"value\":0,\"valid\":0},"
                 "{\"nodeId\":\"%s\",\"type\":\"%s\",\"Temp\":0.00,\"Hum\":0.00,\"value\":0,\"valid\":0},"
                 "{\"nodeId\":\"%s\",\"type\":\"%s\",\"Temp\":0.00,\"Hum\":0.00,\"value\":0,\"valid\":0}]}",
                 WIFI8266_MQTT_CLIENT_ID,
                 (unsigned long)data->sequence,
                 (unsigned long)data->tick,
                 WIFI8266_NODE_ROOM_TH_ID,
                 WIFI8266_NODE_ROOM_TH_TYPE,
                 (long)(data->temperature_c_x100 / 100),
                 (long)(data->temperature_c_x100 >= 0 ? data->temperature_c_x100 % 100 : -(data->temperature_c_x100 % 100)),
                 (long)(data->humidity_rh_x100 / 100),
                 (long)(data->humidity_rh_x100 >= 0 ? data->humidity_rh_x100 % 100 : -(data->humidity_rh_x100 % 100)),
                 (unsigned int)data->valid,
                 WIFI8266_NODE_LIVING_TH_ID,
                 WIFI8266_NODE_LIVING_TH_TYPE,
                 WIFI8266_NODE_ROOM_LIGHT_ID,
                 WIFI8266_NODE_ROOM_LIGHT_TYPE,
                 WIFI8266_NODE_LIVING_LIGHT_ID,
                 WIFI8266_NODE_LIVING_LIGHT_TYPE);

  return wifi8266_mqtt_publish_ex(WIFI8266_TOPIC_NODES_TELEMETRY, payload, TX_FALSE);
}

static UINT wifi8266_mqtt_publish_ex(const char *topic, const char *payload, UINT retain)
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
                 "AT+MQTTPUBRAW=0,\"%s\",%u,0,%u\r\n",
                 topic,
                 (unsigned int)payload_len,
                 retain == TX_TRUE ? 1U : 0U);

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

static UINT wifi8266_send_cmd_hidden(const char *log_text, const char *cmd)
{
  wifi8266_rx_flush();

  if (cmd == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  if (log_text != TX_NULL)
  {
    wifi8266_debug_write(log_text);
  }

  if (HAL_UART_Transmit(&huart1,
                        (uint8_t *)cmd,
                        (uint16_t)strlen(cmd),
                        300U) != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
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

      if (strstr(response, "WIFI DISCONNECT") != TX_NULL)
      {
        wifi8266_wifi_connected = TX_FALSE;
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
