#include "esp8266_mqtt.h"

#include "esp8266.h"
#include "esp8266_at.h"
#include "esp8266_config.h"

#include <stdio.h>
#include <string.h>

static esp8266_mqtt_state_t s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
static esp8266_topic_cb_t s_topic_cb = 0;

static UINT mqtt_send_wait_ok(const char *line, ULONG timeout)
{
  esp8266_at_result_t r;
  UINT s;

  s = esp8266_send_at_line(line, timeout);
  if (s != TX_SUCCESS)
  {
    return s;
  }

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

UINT esp8266_mqtt_init(void)
{
  s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
  s_topic_cb = 0;
  return TX_SUCCESS;
}

UINT esp8266_mqtt_connect(void)
{
  char cmd[320];
  UINT s;
  esp8266_at_result_t r;

  s_mqtt_state = ESP8266_MQTT_CONNECTING;

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTUSERCFG=0,%u,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                 (unsigned int)MQTT_SCHEME_TLS,
                 MQTT_CLIENT_ID,
                 MQTT_USER,
                 MQTT_PASS);
  s = mqtt_send_wait_ok(cmd, ESP8266_CMD_TIMEOUT);
  if (s != TX_SUCCESS)
  {
    s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
    return s;
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTCLIENTID=0,\"%s\"",
                 MQTT_CLIENT_ID);
  /* Some ESP-AT builds already take client-id from MQTTUSERCFG.
     If this command is unsupported, continue with the existing config. */
  (void)mqtt_send_wait_ok(cmd, ESP8266_CMD_TIMEOUT);

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTCONNCFG=0,%u,%u,\"\",\"\",0",
                 (unsigned int)MQTT_KEEPALIVE_SEC,
                 (unsigned int)MQTT_CLEAN_SESSION);
  s = mqtt_send_wait_ok(cmd, ESP8266_CMD_TIMEOUT);
  if (s != TX_SUCCESS)
  {
    s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
    return s;
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTCONN=0,\"%s\",%u,0",
                 MQTT_HOST,
                 (unsigned int)MQTT_PORT);

  s = esp8266_send_at_line(cmd, ESP8266_CMD_TIMEOUT);
  if (s != TX_SUCCESS)
  {
    s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
    return s;
  }

  s = esp8266_at_wait_result(&r,
                             ESP8266_MQTT_CONN_TIMEOUT,
                             ESP8266_AT_FLG_MQTT_CONNECTED | ESP8266_AT_FLG_OK | ESP8266_AT_FLG_ERROR | ESP8266_AT_FLG_FAIL,
                             0);
  if (s != TX_SUCCESS)
  {
    s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
    return s;
  }

  if (!(r.mqtt_connected || r.ok))
  {
    s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
    return TX_NOT_DONE;
  }

  s_mqtt_state = ESP8266_MQTT_CONNECTED;
  return TX_SUCCESS;
}

UINT esp8266_mqtt_disconnect(void)
{
  s_mqtt_state = ESP8266_MQTT_DISCONNECTED;
  return TX_SUCCESS;
}

UINT esp8266_mqtt_publish(const char *topic, const char *payload, uint8_t qos0)
{
  char cmd[384];

  if ((topic == NULL) || (payload == NULL))
  {
    return TX_PTR_ERROR;
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTPUB=0,\"%s\",\"%s\",%u,0",
                 topic,
                 payload,
                 (unsigned int)qos0);

  return mqtt_send_wait_ok(cmd, ESP8266_CMD_TIMEOUT);
}

UINT esp8266_mqtt_subscribe(const char *topic, uint8_t qos0)
{
  char cmd[200];

  if (topic == NULL)
  {
    return TX_PTR_ERROR;
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+MQTTSUB=0,\"%s\",%u",
                 topic,
                 (unsigned int)qos0);

  return mqtt_send_wait_ok(cmd, ESP8266_CMD_TIMEOUT);
}

UINT esp8266_mqtt_reconnect(void)
{
  return esp8266_mqtt_connect();
}

void esp8266_mqtt_set_topic_callback(esp8266_topic_cb_t cb)
{
  s_topic_cb = cb;
}

esp8266_mqtt_state_t esp8266_mqtt_state_get(void)
{
  return s_mqtt_state;
}

void esp8266_mqtt_notify_rx_topic(const char *topic, const char *payload, uint16_t len)
{
  if (s_topic_cb != 0)
  {
    s_topic_cb(topic, payload, len);
  }
}
