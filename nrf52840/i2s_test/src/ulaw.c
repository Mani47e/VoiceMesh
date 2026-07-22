#include "ulaw.h"

#define ULAW_BIAS 0x84
#define ULAW_CLIP 32635

static const int16_t seg_end[8] =
{
    0xFF,
    0x1FF,
    0x3FF,
    0x7FF,
    0xFFF,
    0x1FFF,
    0x3FFF,
    0x7FFF
};

static int search(int16_t val)
{
    for(int i=0;i<8;i++)
    {
        if(val<=seg_end[i])
            return i;
    }

    return 8;
}

uint8_t ulaw_encode(int16_t pcm)
{
    uint8_t mask;
    uint8_t seg;
    uint8_t uval;

    if(pcm<0)
    {
        pcm=-pcm;
        mask=0x7F;
    }
    else
    {
        mask=0xFF;
    }

    if(pcm>ULAW_CLIP)
        pcm=ULAW_CLIP;

    pcm+=ULAW_BIAS;

    seg=search(pcm);

    uval=(seg<<4)|((pcm>>(seg+3))&0x0F);

    return uval^mask;
}

int16_t ulaw_decode(uint8_t u_val)
{
    u_val=~u_val;

    int t=((u_val&0x0F)<<3)+ULAW_BIAS;
    t<<=((u_val&0x70)>>4);

    if(u_val&0x80)
        return t-ULAW_BIAS;

    return ULAW_BIAS-t;
}