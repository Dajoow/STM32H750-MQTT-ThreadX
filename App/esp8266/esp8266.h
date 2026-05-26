#ifndef ESP8266_H
#define ESP8266_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp8266_config.h"
#include "esp8266_mqtt.h"
#include "tx_api.h"
#include <stdint.h>

/* Driver-level event flags */
#define ESP8266_DRV_EVT_RX_READY          (1UL << 0)
#define ESP8266_DRV_EVT_RECONNECT         (1UL << 1)
#define ESP8266_DRV_EVT_MQTT_RX           (1UL << 2)
#define ESP8266_DRV_EVT_TX_DONE           (1UL << 3)

/* Core service control */
UINT esp8266_service_init(TX_BYTE_POOL *pool);

/* ISR hooks */
void esp8266_uart_rx_event_isr(void);
void esp8266_uart_rx_half_isr(void);
void esp8266_uart_rx_cplt_isr(void);
void esp8266_uart_tx_cplt_isr(void);

/* AT send API */
UINT esp8266_send_at_line(const char *line, ULONG timeout);

/* User-level publish API */
UINT esp8266_cloud_publish(const char *topic, const char *payload);

#ifdef __cplusplus
}
#endif

#endif
