#ifndef ESP8266_RINGBUF_H
#define ESP8266_RINGBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint8_t *buf;
  uint32_t size;
  volatile uint32_t head;
  volatile uint32_t tail;
  volatile uint32_t overflow_cnt;
} esp8266_ringbuf_t;

void esp8266_ringbuf_init(esp8266_ringbuf_t *rb, uint8_t *mem, uint32_t size);
uint32_t esp8266_ringbuf_write(esp8266_ringbuf_t *rb, const uint8_t *data, uint32_t len);
uint32_t esp8266_ringbuf_read(esp8266_ringbuf_t *rb, uint8_t *data, uint32_t len);
uint32_t esp8266_ringbuf_available(const esp8266_ringbuf_t *rb);
bool esp8266_ringbuf_is_empty(const esp8266_ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif
