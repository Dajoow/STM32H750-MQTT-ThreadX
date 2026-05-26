#include "esp8266_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
  DEC_IDLE = 0,
  DEC_MQTT_SUBRECV
} decode_state_t;

static decode_state_t parser_identify(const char *line)
{
  if (strcmp(line, "OK") == 0)
  {
    return DEC_IDLE;
  }

  if (strstr(line, "+MQTTSUBRECV:") != NULL)
  {
    return DEC_MQTT_SUBRECV;
  }

  return DEC_IDLE;
}

static void parser_decode_subrecv(const char *line, esp8266_urc_t *urc)
{
  const char *p;
  const char *topic_b;
  const char *topic_e;
  const char *q;
  const char *payload_b;
  const char *payload_e;
  size_t tlen;
  size_t plen;

  p = strstr(line, "+MQTTSUBRECV:");
  if (p == NULL)
  {
    urc->type = ESP8266_URC_UNKNOWN;
    return;
  }

  topic_b = strchr(p, '"');
  if (topic_b == NULL)
  {
    urc->type = ESP8266_URC_UNKNOWN;
    return;
  }
  topic_b++;

  topic_e = strchr(topic_b, '"');
  if (topic_e == NULL)
  {
    urc->type = ESP8266_URC_UNKNOWN;
    return;
  }

  q = strchr(topic_e + 1, ',');
  if (q == NULL)
  {
    urc->type = ESP8266_URC_UNKNOWN;
    return;
  }
  q = strchr(q + 1, ',');
  if (q == NULL)
  {
    urc->type = ESP8266_URC_UNKNOWN;
    return;
  }
  payload_b = strchr(q + 1, '"');
  if (payload_b == NULL)
  {
    urc->type = ESP8266_URC_UNKNOWN;
    return;
  }
  payload_b++;
  payload_e = strrchr(payload_b, '"');
  if ((payload_e == NULL) || (payload_e < payload_b))
  {
    urc->type = ESP8266_URC_UNKNOWN;
    return;
  }

  tlen = (size_t)(topic_e - topic_b);
  if (tlen >= sizeof(urc->topic))
  {
    tlen = sizeof(urc->topic) - 1U;
  }

  plen = (size_t)(payload_e - payload_b);
  if (plen >= sizeof(urc->payload))
  {
    plen = sizeof(urc->payload) - 1U;
  }

  memcpy(urc->topic, topic_b, tlen);
  urc->topic[tlen] = '\0';

  memcpy(urc->payload, payload_b, plen);
  urc->payload[plen] = '\0';
  urc->payload_len = (uint16_t)plen;
  urc->type = ESP8266_URC_MQTT_SUBRECV;
}

void esp8266_parser_init(esp8266_line_parser_t *p)
{
  if (p == NULL)
  {
    return;
  }

  memset(p, 0, sizeof(*p));
}

bool esp8266_parser_feed(esp8266_line_parser_t *p,
                         uint8_t ch,
                         esp8266_urc_t *urc,
                         bool *ready)
{
  if ((p == NULL) || (urc == NULL) || (ready == NULL))
  {
    return false;
  }

  *ready = false;

  if (ch == '\r')
  {
    return true;
  }

  if (ch == '>')
  {
    memset(urc, 0, sizeof(*urc));
    urc->type = ESP8266_URC_PROMPT;
    strcpy(urc->line, ">");
    *ready = true;
    return true;
  }

  if (ch == '\n')
  {
    if (p->len == 0U)
    {
      return true;
    }

    p->line[p->len] = '\0';

    memset(urc, 0, sizeof(*urc));
    esp8266_parser_decode_line(p->line, urc);
    p->len = 0U;
    *ready = true;
    return true;
  }

  if (p->len < (uint16_t)(sizeof(p->line) - 1U))
  {
    p->line[p->len++] = (char)ch;
  }
  else
  {
    p->len = 0U;
    return false;
  }

  return true;
}

void esp8266_parser_decode_line(const char *line, esp8266_urc_t *urc)
{
  decode_state_t st;

  if ((line == NULL) || (urc == NULL))
  {
    return;
  }

  memset(urc, 0, sizeof(*urc));
  strncpy(urc->line, line, sizeof(urc->line) - 1U);
  urc->line[sizeof(urc->line) - 1U] = '\0';

  st = parser_identify(line);
  if (st == DEC_MQTT_SUBRECV)
  {
    parser_decode_subrecv(line, urc);
    return;
  }

  if (strcmp(line, "OK") == 0)
  {
    urc->type = ESP8266_URC_OK;
  }
  else if (strcmp(line, "ERROR") == 0)
  {
    urc->type = ESP8266_URC_ERROR;
  }
  else if (strcmp(line, "FAIL") == 0)
  {
    urc->type = ESP8266_URC_FAIL;
  }
  else if (strstr(line, "busy p") != NULL)
  {
    urc->type = ESP8266_URC_BUSY;
  }
  else if (strcmp(line, "ready") == 0)
  {
    urc->type = ESP8266_URC_READY;
  }
  else if (strcmp(line, "WIFI CONNECTED") == 0)
  {
    urc->type = ESP8266_URC_WIFI_CONNECTED;
  }
  else if ((strcmp(line, "WIFI DISCONNECT") == 0) || (strcmp(line, "WIFI DISCONNECTED") == 0))
  {
    urc->type = ESP8266_URC_WIFI_DISCONNECT;
  }
  else if (strcmp(line, "WIFI GOT IP") == 0)
  {
    urc->type = ESP8266_URC_WIFI_GOT_IP;
  }
  else if (strstr(line, "+MQTTCONNECTED:") != NULL)
  {
    urc->type = ESP8266_URC_MQTT_CONNECTED;
  }
  else if (strstr(line, "+MQTTDISCONNECTED:") != NULL)
  {
    urc->type = ESP8266_URC_MQTT_DISCONNECTED;
  }
  else if (strstr(line, "CLOSED") != NULL)
  {
    urc->type = ESP8266_URC_CLOSED;
  }
  else
  {
    urc->type = ESP8266_URC_UNKNOWN;
  }
}
