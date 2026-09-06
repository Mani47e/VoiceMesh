#ifndef ULAW_H
#define ULAW_H

#include <stdint.h>

uint8_t ulaw_encode(int16_t sample);
int16_t ulaw_decode(uint8_t sample);

#endif