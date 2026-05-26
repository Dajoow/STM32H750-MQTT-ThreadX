#ifndef ESP8266_PARSER_H
#define ESP8266_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp8266_config.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  ESP8266_URC_NONE = 0,
  ESP8266_URC_OK,
  ESP8266_URC_ERROR,
  ESP8266_URC_FAIL,
  ESP8266_URC_BUSY,
  ESP8266_URC_READY,
  ESP8266_URC_PROMPT,
  ESP8266_URC_WIFI_CONNECTED,
  ESP8266_URC_WIFI_DISCONNECT,
  ESP8266_URC_WIFI_GOT_IP,
  ESP8266_URC_MQTT_CONNECTED,
  ESP8266_URC_MQTT_DISCONNECTED,
  ESP8266_URC_MQTT_SUBRECV,
  ESP8266_URC_CLOSED,
  ESP8266_URC_UNKNOWN
} esp8266_urc_type_t;

typedef struct
{
  esp8266_urc_type_t type;
  uint32_t tick;
  char line[ESP8266_AT_LINE_MAX];
  char topic[ESP8266_TOPIC_MAX];
  char payload[ESP8266_PAYLOAD_MAX];
  uint16_t payload_len;
} esp8266_urc_t;

typedef struct
{
  char line[ESP8266_AT_LINE_MAX];
  uint16_t len;
} esp8266_line_parser_t;

void esp8266_parser_init(esp8266_line_parser_t *p);
bool esp8266_parser_feed(esp8266_line_parser_t *p,
                         uint8_t ch,
                         esp8266_urc_t *urc,
                         bool *ready);
void esp8266_parser_decode_line(const char *line, esp8266_urc_t *urc);

#ifdef __cplusplus
}
#endif

#endif
