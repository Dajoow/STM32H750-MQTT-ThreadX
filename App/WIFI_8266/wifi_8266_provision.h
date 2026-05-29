#ifndef WIFI_8266_PROVISION_H
#define WIFI_8266_PROVISION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdint.h>

#define WIFI8266_PROV_SSID_MAX 32U
#define WIFI8266_PROV_PASS_MAX 64U

typedef struct
{
  char ssid[WIFI8266_PROV_SSID_MAX + 1U];
  char password[WIFI8266_PROV_PASS_MAX + 1U];
  uint8_t valid;
} wifi8266_provision_config_t;

UINT wifi8266_provision_run(wifi8266_provision_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_8266_PROVISION_H */
