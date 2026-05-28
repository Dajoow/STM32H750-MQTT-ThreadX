#include "sensor_data.h"

static uint32_t sensor_sequence;

UINT sensor_data_init(void)
{
  sensor_sequence = 0U;
  return TX_SUCCESS;
}

UINT sensor_data_read(sensor_data_t *data)
{
  if (data == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  sensor_sequence++;

  /* Placeholder data source. Replace this block with real I2C/ADC sensor reads. */
  data->sequence = sensor_sequence;
  data->tick = (uint32_t)tx_time_get();
  data->temperature_c_x100 = 2500 + (int32_t)(sensor_sequence % 50U);
  data->humidity_rh_x100 = 6000 + (int32_t)((sensor_sequence * 3U) % 100U);
  data->raw_value = (uint16_t)(1000U + (sensor_sequence % 500U));
  data->valid = 1U;

  return TX_SUCCESS;
}
