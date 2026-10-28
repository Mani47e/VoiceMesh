#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_SIZE 8000

void ring_init(void);
void ring_reset(void);
int  ring_available(void);
bool ring_push(uint8_t b);
bool ring_pop(uint8_t *b);

#endif /* RINGBUFFER_H */