#include "esp8266_ringbuf.h"

#include <stddef.h>

static uint32_t rb_next(const esp8266_ringbuf_t *rb, uint32_t index)
{
  return (index + 1U) % rb->size;
}

void esp8266_ringbuf_init(esp8266_ringbuf_t *rb, uint8_t *mem, uint32_t size)
{
  if ((rb == NULL) || (mem == NULL) || (size < 2U))
  {
    return;
  }

  rb->buf = mem;
  rb->size = size;
  rb->head = 0U;
  rb->tail = 0U;
  rb->overflow_cnt = 0U;
}

uint32_t esp8266_ringbuf_write(esp8266_ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
  uint32_t i;
  uint32_t next;
  uint32_t wrote = 0U;

  if ((rb == NULL) || (data == NULL))
  {
    return 0U;
  }

  for (i = 0U; i < len; i++)
  {
    next = rb_next(rb, rb->head);
    if (next == rb->tail)
    {
      rb->overflow_cnt++;
      break;
    }

    rb->buf[rb->head] = data[i];
    rb->head = next;
    wrote++;
  }

  return wrote;
}

uint32_t esp8266_ringbuf_read(esp8266_ringbuf_t *rb, uint8_t *data, uint32_t len)
{
  uint32_t count = 0U;

  if ((rb == NULL) || (data == NULL))
  {
    return 0U;
  }

  while ((rb->tail != rb->head) && (count < len))
  {
    data[count++] = rb->buf[rb->tail];
    rb->tail = rb_next(rb, rb->tail);
  }

  return count;
}

uint32_t esp8266_ringbuf_available(const esp8266_ringbuf_t *rb)
{
  if (rb == NULL)
  {
    return 0U;
  }

  if (rb->head >= rb->tail)
  {
    return rb->head - rb->tail;
  }

  return (rb->size - rb->tail + rb->head);
}

bool esp8266_ringbuf_is_empty(const esp8266_ringbuf_t *rb)
{
  if (rb == NULL)
  {
    return true;
  }

  return (rb->head == rb->tail);
}
