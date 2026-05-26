#ifndef ESP8266_AT_H
#define ESP8266_AT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp8266_parser.h"
#include "tx_api.h"
#include <stdbool.h>
#include <stdint.h>

/* AT event flags */
#define ESP8266_AT_FLG_OK                 (1UL << 0)
#define ESP8266_AT_FLG_ERROR              (1UL << 1)
#define ESP8266_AT_FLG_FAIL               (1UL << 2)
#define ESP8266_AT_FLG_BUSY               (1UL << 3)
#define ESP8266_AT_FLG_WIFI_CONNECTED     (1UL << 4)
#define ESP8266_AT_FLG_WIFI_DISCONNECT    (1UL << 5)
#define ESP8266_AT_FLG_WIFI_GOT_IP        (1UL << 6)
#define ESP8266_AT_FLG_MQTT_CONNECTED     (1UL << 7)
#define ESP8266_AT_FLG_MQTT_DISCONNECTED  (1UL << 8)
#define ESP8266_AT_FLG_CLOSED             (1UL << 9)
#define ESP8266_AT_FLG_READY              (1UL << 10)
#define ESP8266_AT_FLG_PROMPT             (1UL << 11)

typedef struct
{
  bool ok;
  bool error;
  bool fail;
  bool busy;
  bool wifi_connected;
  bool wifi_disconnect;
  bool wifi_got_ip;
  bool mqtt_connected;
  bool mqtt_disconnected;
  bool closed;
  bool ready;
  bool prompt;
} esp8266_at_result_t;

UINT esp8266_at_core_init(void);
void esp8266_at_core_on_rx(const uint8_t *data, uint16_t len);
UINT esp8266_at_wait_result(esp8266_at_result_t *res,
                            ULONG wait_ticks,
                            ULONG expect_flags,
                            ULONG *actual_flags);
UINT esp8266_at_get_urc(esp8266_urc_t **urc, ULONG wait);
void esp8266_at_release_urc(esp8266_urc_t *urc);
void esp8266_at_reset_flags(void);
TX_EVENT_FLAGS_GROUP *esp8266_at_flags_group(void);

#ifdef __cplusplus
}
#endif

#endif
