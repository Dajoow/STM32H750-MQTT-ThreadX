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
#define WIFI8266_TOPIC_PREFIX       "stm32h750/esp8266_001"
#define WIFI8266_TOPIC_TELEMETRY    WIFI8266_TOPIC_PREFIX "/telemetry"
#define WIFI8266_TOPIC_STATUS       WIFI8266_TOPIC_PREFIX "/status"
#define WIFI8266_TOPIC_AVAILABILITY WIFI8266_TOPIC_PREFIX "/availability"
#define WIFI8266_TOPIC_COMMAND      WIFI8266_TOPIC_PREFIX "/command"
#define WIFI8266_TOPIC_META_NODES   WIFI8266_TOPIC_PREFIX "/meta/nodes"
#define WIFI8266_TOPIC_NODES_TELEMETRY WIFI8266_TOPIC_PREFIX "/nodes/telemetry"

/* Preset nodes for Web/App discovery. */
#define WIFI8266_NODE_ROOM_TH_ID              "room_temp_humi"
#define WIFI8266_NODE_ROOM_TH_NAME            "Room Temp/Humi"
#define WIFI8266_NODE_ROOM_TH_TYPE            "dht11"
#define WIFI8266_NODE_ROOM_TH_TELEMETRY_TOPIC WIFI8266_TOPIC_PREFIX "/nodes/" WIFI8266_NODE_ROOM_TH_ID "/telemetry"
#define WIFI8266_NODE_ROOM_TH_STATUS_TOPIC    WIFI8266_TOPIC_PREFIX "/nodes/" WIFI8266_NODE_ROOM_TH_ID "/status"
#define WIFI8266_NODE_ROOM_TH_COMMAND_TOPIC   WIFI8266_TOPIC_PREFIX "/nodes/" WIFI8266_NODE_ROOM_TH_ID "/command"

#define WIFI8266_NODE_LIVING_TH_ID            "living_temp_humi"
#define WIFI8266_NODE_LIVING_TH_NAME          "Living Temp/Humi"
#define WIFI8266_NODE_LIVING_TH_TYPE          "temp_humi"
#define WIFI8266_NODE_LIVING_TH_STATUS_TOPIC  WIFI8266_TOPIC_PREFIX "/nodes/" WIFI8266_NODE_LIVING_TH_ID "/status"

#define WIFI8266_NODE_ROOM_LIGHT_ID           "room_light"
#define WIFI8266_NODE_ROOM_LIGHT_NAME         "Room Light"
#define WIFI8266_NODE_ROOM_LIGHT_TYPE         "light"
#define WIFI8266_NODE_ROOM_LIGHT_STATUS_TOPIC WIFI8266_TOPIC_PREFIX "/nodes/" WIFI8266_NODE_ROOM_LIGHT_ID "/status"

#define WIFI8266_NODE_LIVING_LIGHT_ID           "living_light"
#define WIFI8266_NODE_LIVING_LIGHT_NAME         "Living Light"
#define WIFI8266_NODE_LIVING_LIGHT_TYPE         "light"
#define WIFI8266_NODE_LIVING_LIGHT_STATUS_TOPIC WIFI8266_TOPIC_PREFIX "/nodes/" WIFI8266_NODE_LIVING_LIGHT_ID "/status"

#define WIFI8266_TOPIC_NODE_COMMANDS          WIFI8266_TOPIC_PREFIX "/nodes/+/command"

#define WIFI8266_TELEMETRY_PERIOD_TICKS 2000U

/* DHT11 one-wire data pin is defined in Core/Inc/main.h:
 * DTH11_Pin / DTH11_GPIO_Port.
 */
#define SENSOR_DQ_SELF_TEST_ENABLE 0U

#endif /* WIFI_8266_CONFIG_H */
