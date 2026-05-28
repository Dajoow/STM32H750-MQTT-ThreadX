#include "sensor_data.h"

#include "dht11.h"
#include "wifi_8266_config.h"
#include <string.h>

static uint32_t sensor_sequence;

static void sensor_dwt_init(void);
static void sensor_delay_us(uint32_t us);
static uint16_t sensor_dq_self_test(void);

UINT sensor_data_init(void)
{
  sensor_sequence = 0U;
  sensor_dwt_init();


  return TX_SUCCESS;
}

UINT sensor_data_read(sensor_data_t *data)
{
  float temperature_c;
  float humidity_rh;
  uint8_t am2320_status;

  if (data == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  sensor_sequence++;
  (void)memset(data, 0, sizeof(*data));

  data->sequence = sensor_sequence;
  data->tick = (uint32_t)tx_time_get();

#if (SENSOR_DQ_SELF_TEST_ENABLE != 0U)
  data->debug_value = sensor_dq_self_test();
  data->raw_value = data->debug_value;
  data->valid = 0U;
  return TX_SUCCESS;
#endif


  if (am2320_status == 0U)
  {
    data->temperature_c_x100 = (int32_t)(temperature_c * 100.0f);
    data->humidity_rh_x100 = (int32_t)(humidity_rh * 100.0f);
    data->raw_value = 0U;
    data->valid = 1U;
  }
  else
  {
    data->temperature_c_x100 = 0;
    data->humidity_rh_x100 = 0;
    data->raw_value = am2320_status;
    data->valid = 0U;
  }

  return TX_SUCCESS;
}

static void sensor_dwt_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void sensor_delay_us(uint32_t us)
{
  const uint32_t start = DWT->CYCCNT;
  const uint32_t ticks = (SystemCoreClock / 1000000U) * us;

  while ((DWT->CYCCNT - start) < ticks)
  {
  }
}

static uint16_t sensor_dq_self_test(void)
{
  uint8_t idle_level;
  uint8_t drive_low_level;
  uint8_t release_100us_level;
  uint8_t release_1ms_level;



  return (uint16_t)(50000U +
                    ((uint16_t)idle_level * 1000U) +
                    ((uint16_t)drive_low_level * 100U) +
                    ((uint16_t)release_100us_level * 10U) +
                    (uint16_t)release_1ms_level);
}
