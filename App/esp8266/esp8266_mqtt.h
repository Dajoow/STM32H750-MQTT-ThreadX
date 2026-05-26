#ifndef ESP8266_MQTT_H
#define ESP8266_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  ESP8266_MQTT_DISCONNECTED = 0,
  ESP8266_MQTT_CONNECTING,
  ESP8266_MQTT_CONNECTED
} esp8266_mqtt_state_t;

typedef void (*esp8266_topic_cb_t)(const char *topic, const char *payload, uint16_t len);

UINT esp8266_mqtt_init(void);
UINT esp8266_mqtt_connect(void);
UINT esp8266_mqtt_disconnect(void);
UINT esp8266_mqtt_publish(const char *topic, const char *payload, uint8_t qos0);
UINT esp8266_mqtt_subscribe(const char *topic, uint8_t qos0);
UINT esp8266_mqtt_reconnect(void);
void esp8266_mqtt_set_topic_callback(esp8266_topic_cb_t cb);
esp8266_mqtt_state_t esp8266_mqtt_state_get(void);
void esp8266_mqtt_notify_rx_topic(const char *topic, const char *payload, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
