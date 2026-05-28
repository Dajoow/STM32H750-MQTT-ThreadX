#ifndef WIFI_8266_CONFIG_H
#define WIFI_8266_CONFIG_H

/* Local credentials are intentionally not tracked by git.
 * Create App/WIFI_8266/wifi_8266_secrets.h from wifi_8266_secrets.example.h.
 */
#include "wifi_8266_secrets.h"

/* HiveMQ Cloud common config. */
#define WIFI8266_MQTT_SCHEME     "2"   /* 1: TCP, 2: TLS without cert verify on ESP-AT */
#define WIFI8266_MQTT_PORT       "8883"
#define WIFI8266_MQTT_CLIENT_ID  "stm32h750_esp8266_001"

/* Web/App interface topics. Both web and app can subscribe/publish these. */
#define WIFI8266_TOPIC_TELEMETRY "stm32h750/esp8266_001/telemetry"
#define WIFI8266_TOPIC_STATUS    "stm32h750/esp8266_001/status"
#define WIFI8266_TOPIC_COMMAND   "stm32h750/esp8266_001/command"

#define WIFI8266_TELEMETRY_PERIOD_TICKS 2000U

/* AM2320 one-wire data pin is defined in Core/Inc/main.h:
 * AM2320_Pin / AM2320_GPIO_Port.
 */
#define SENSOR_DQ_SELF_TEST_ENABLE 1U

#endif /* WIFI_8266_CONFIG_H */
