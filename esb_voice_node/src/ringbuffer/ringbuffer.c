#include "ringbuffer.h"

static uint8_t ring[RING_SIZE];
static volatile int ring_head = 0;
static volatile int ring_tail = 0;

void ring_init(void)  { ring_reset(); }
void ring_reset(void) { ring_head = 0; ring_tail = 0; }

int ring_available(void)
{
        return (ring_head - ring_tail + RING_SIZE) % RING_SIZE;
}

bool ring_push(uint8_t b)
{
        int next = (ring_head + 1) % RING_SIZE;
        if (next != ring_tail) {
                ring[ring_head] = b;
                ring_head = next;
                return true;
        }
        return false;
}

bool ring_pop(uint8_t *b)
{
        if (ring_head == ring_tail) {
                return false;
        }
        *b = ring[ring_tail];
        ring_tail = (ring_tail + 1) % RING_SIZE;
        return true;
}