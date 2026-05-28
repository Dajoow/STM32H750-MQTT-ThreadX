#ifndef WIFI_8266_H
#define WIFI_8266_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdint.h>

#define WIFI8266_THREAD_STACK_SIZE       3072U
#define WIFI8266_LOG_THREAD_STACK_SIZE   1024U
#define WIFI8266_BLINK_THREAD_STACK_SIZE 1024U

extern volatile UINT wifi8266_blink_thread_count;
extern volatile UINT wifi8266_log_thread_count;
extern volatile UINT wifi8266_service_thread_count;
extern volatile UINT wifi8266_rx_overflow_count;
extern volatile UINT wifi8266_publish_count;
extern volatile UINT wifi8266_command_count;

void wifi8266_blink_thread_entry(ULONG thread_input);
void wifi8266_log_thread_entry(ULONG thread_input);
void wifi8266_service_thread_entry(ULONG thread_input);
void wifi8266_debug_write(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_8266_H */
