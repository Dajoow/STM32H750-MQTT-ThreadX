#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdint.h>

typedef struct
{
  uint32_t sequence;
  uint32_t tick;
  int32_t temperature_c_x100;
  int32_t humidity_rh_x100;
  uint16_t raw_value;
  uint8_t valid;
} sensor_data_t;

UINT sensor_data_init(void);
UINT sensor_data_read(sensor_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_DATA_H */
