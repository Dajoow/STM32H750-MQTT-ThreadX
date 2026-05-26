#ifndef ESP8266_CONFIG_H
#define ESP8266_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =============================
 * WiFi / HiveMQ Cloud Config
 * ============================= */
#define WIFI_SSID                 "YOUR_WIFI_SSID"
#define WIFI_PASSWORD             "YOUR_WIFI_PASSWORD"

#define MQTT_HOST                 "YOUR_CLUSTER.s1.eu.hivemq.cloud"
#define MQTT_PORT                 8883U
#define MQTT_USER                 "YOUR_HIVEMQ_USER"
#define MQTT_PASS                 "YOUR_HIVEMQ_PASSWORD"
#define MQTT_CLIENT_ID            "stm32h750-esp8266"

#define MQTT_SUB_TOPIC            "stm32/h750/cmd"
#define MQTT_PUB_TOPIC            "stm32/h750/status"
#define MQTT_KEEPALIVE_SEC        60U
#define MQTT_CLEAN_SESSION        1U
#define MQTT_QOS0                 0U

/* ESP-AT SSL scheme: 2 means MQTT over TLS in common ESP-AT releases. */
#define MQTT_SCHEME_TLS           2U

/* =============================
 * Buffer / Queue / Task Config
 * ============================= */
#define ESP8266_UART_BAUDRATE           115200U
#define ESP8266_DMA_RX_BUF_SIZE         512U
#define ESP8266_RINGBUF_SIZE            4096U
#define ESP8266_AT_LINE_MAX             256U
#define ESP8266_AT_EVENT_POOL_SIZE      24U
#define ESP8266_TX_ITEM_POOL_SIZE       16U
#define ESP8266_TOPIC_MAX               128U
#define ESP8266_PAYLOAD_MAX             256U

#define ESP8266_RX_THREAD_STACK         4096U
#define ESP8266_MQTT_THREAD_STACK       4096U
#define ESP8266_HEARTBEAT_THREAD_STACK  2048U

#define ESP8266_RX_THREAD_PRIO          10U
#define ESP8266_MQTT_THREAD_PRIO        11U
#define ESP8266_HEARTBEAT_THREAD_PRIO   12U

#define ESP8266_TICK_1S                 1000U

/* =============================
 * Timeout Config (tick)
 * ============================= */
#define ESP8266_CMD_TIMEOUT             (5U * ESP8266_TICK_1S)
#define ESP8266_WIFI_JOIN_TIMEOUT       (30U * ESP8266_TICK_1S)
#define ESP8266_MQTT_CONN_TIMEOUT       (20U * ESP8266_TICK_1S)
#define ESP8266_RECONNECT_PERIOD        (3U * ESP8266_TICK_1S)
#define ESP8266_HEARTBEAT_PERIOD        (15U * ESP8266_TICK_1S)
#define ESP8266_TX_TIMEOUT              (3U * ESP8266_TICK_1S)

#ifdef __cplusplus
}
#endif

#endif
