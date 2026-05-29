#include "wifi_8266_provision.h"

#include "usart.h"
#include <string.h>

static void provision_write(const char *text);
static UINT provision_read_line(char *buf, uint16_t max_len, uint8_t echo_star);

UINT wifi8266_provision_run(wifi8266_provision_config_t *config)
{
  if (config == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  (void)memset(config, 0, sizeof(*config));

  provision_write("\r\n================================\r\n");
  provision_write(" STM32H750 ESP8266 WiFi Setup\r\n");
  provision_write("================================\r\n");
  provision_write("Input SSID and password to connect WiFi.\r\n");
  provision_write("Press ENTER on SSID to use default saved WiFi.\r\n\r\n");

  provision_write("SSID: ");
  if (provision_read_line(config->ssid, sizeof(config->ssid), 0U) != TX_SUCCESS)
  {
    provision_write("\r\n[WIFI SETUP] SSID input failed, use default WiFi.\r\n");
    return TX_NOT_DONE;
  }

  if (config->ssid[0] == '\0')
  {
    provision_write("[WIFI SETUP] Use default saved WiFi.\r\n");
    return TX_NOT_DONE;
  }

  provision_write("PASS: ");
  if (provision_read_line(config->password, sizeof(config->password), 1U) != TX_SUCCESS)
  {
    provision_write("\r\n[WIFI SETUP] Password input failed, use default WiFi.\r\n");
    (void)memset(config, 0, sizeof(*config));
    return TX_NOT_DONE;
  }

  config->valid = 1U;
  provision_write("[WIFI SETUP] WiFi config accepted.\r\n");

  return TX_SUCCESS;
}

static void provision_write(const char *text)
{
  if (text == TX_NULL)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart3,
                          (uint8_t *)text,
                          (uint16_t)strlen(text),
                          100U);
}

static UINT provision_read_line(char *buf, uint16_t max_len, uint8_t echo_star)
{
  uint16_t len = 0U;
  uint8_t ch;

  if ((buf == TX_NULL) || (max_len == 0U))
  {
    return TX_PTR_ERROR;
  }

  buf[0] = '\0';

  for (;;)
  {
    if (HAL_UART_Receive(&huart3, &ch, 1U, 100U) != HAL_OK)
    {
      tx_thread_sleep(1U);
      continue;
    }

    if ((ch == '\r') || (ch == '\n'))
    {
      if (ch == '\r')
      {
        uint8_t next_ch;
        (void)HAL_UART_Receive(&huart3, &next_ch, 1U, 5U);
      }

      provision_write("\r\n");
      buf[len] = '\0';
      return TX_SUCCESS;
    }

    if ((ch == '\b') || (ch == 0x7FU))
    {
      if (len > 0U)
      {
        len--;
        buf[len] = '\0';
        provision_write("\b \b");
      }
      continue;
    }

    if ((ch < 0x20U) || (ch > 0x7EU))
    {
      continue;
    }

    if (len < (max_len - 1U))
    {
      buf[len++] = (char)ch;
      buf[len] = '\0';

      if (echo_star != 0U)
      {
        provision_write("*");
      }
      else
      {
        (void)HAL_UART_Transmit(&huart3, &ch, 1U, 20U);
      }
    }
  }
}
